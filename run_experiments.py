# Import python modules
import numpy as np
import os
import math
import copy
from types import SimpleNamespace
import threading
import io
import sys
import traceback
from contextlib import redirect_stdout, redirect_stderr
from concurrent.futures import ThreadPoolExecutor


# Import custom modules
import config as cfg
import helpers as hlp
import run_booksim as rb
import run_orion as ro
import generate_network as gn
import compute_link_power as clp
import fault_insertion as fi


def run_single_experiment(experiment : dict, verbose : bool = False, debug_mode : bool = False) -> None:
    # Export relevant experiment parameters
    exp = SimpleNamespace(**experiment)
    # Check if results exist, if so, skip
    filename = "results/results_%s.json" % exp.name
    if os.path.exists(filename) and not debug_mode:
        hlp.print_yellow("Results for experiment %s already exist, skipping..." % experiment['name'])
        return
    # Results do not exist, proceed
    # Generate the network
    network = gn.generate_network(exp.n_routers, exp.topology, exp.nodes_per_router, exp.link_latency_function, exp.node_link_latency, exp.width, exp.height)
    # Insert faults into the network if configured
    if exp.link_fault_rate > 0.0:
        network = fi.insert_link_faults(network, exp.link_fault_rate, cfg.fault_random_seed, cfg.fault_attempts)
        if network is None:
            hlp.print_red(f"Failed to insert enough link faults for experiment {exp.name}. Skipping...")
            return
    if exp.router_fault_rate > 0.0:
        network = fi.insert_router_faults(network, exp.router_fault_rate, cfg.fault_random_seed, cfg.fault_attempts)
        if network is None:
            hlp.print_red(f"Failed to insert enough router faults for experiment {exp.name}. Skipping...")
            return
    nw = SimpleNamespace(**network)
    # Fetch and adjust BookSim configuration
    booksim_config = copy.deepcopy(cfg.default_booksim_config)
    # Insert experiment-specific parameters for BookSim
    booksim_config["routing_function"] = exp.routing_function
    booksim_config["modular_routing_function"] = exp.mod_routing_function
    booksim_config['modular_selection_function'] = exp.mod_selection_function
    booksim_config["num_vcs"] = exp.num_vcs
    booksim_config["vc_buf_size"] = exp.vc_buf_size
    booksim_config["packet_size"] = exp.packet_size
    booksim_config["message_size"] = exp.message_size
    booksim_config["in_order_ratio"] = exp.in_order_ratio
    booksim_config["n_entropy_bits"] = exp.n_entropy_bits
    # Only overwrite seed if specified in the experiment, otherwise use the default from config.py
    if "seed" in experiment:
        booksim_config["seed"] = experiment["seed"]
    # Overwrite the default sample period if specified in the experiment
    if "sample_period" in experiment:
        booksim_config["sample_period"] = experiment["sample_period"]
    # Set the threshold_mode to the one proposed in the corresponding paper (only required for L-Turn and Left/Right)
    if exp.mod_routing_function in ["lturn","left_right"]:
        booksim_config["threshold_mode"] = "bounded"
    elif exp.mod_routing_function in ["up_down"]:
        booksim_config["threshold_mode"] = "shortest"
    # Handling of trace-based vs traffic-based simulations
    bs_mode = "traffic"
    bs_traffic = "uniform" # I think this needs a default value, even if not used
    bs_trace_file = None
    bs_sample_period = -1
    if exp.traffic.startswith("trace"):
        bs_mode = "trace"
        trace_cfg = exp.traffic.split("_")
        if len(trace_cfg) != 3:
            hlp.print_error(f"Trace name must be in format trace_<cpu|gpu>_<tracename>, got {exp.traffic}")
            sys.exit(1)
        trace_type = trace_cfg[1]
        trace_name = trace_cfg[2]
        bs_trace_file = "booksim2/src/traces/%s.json" % trace_name
        bs_sample_period = cfg.trace_sample_period
    elif exp.traffic == "debug":
        bs_mode = "debug"
        bs_traffic = "uniform"
        bs_sample_period = 0
    else:
        bs_traffic = exp.traffic
        bs_sample_period = cfg.traffic_sample_period
    # Set BookSim config based on traffic vs trace
    booksim_config['mode'] = bs_mode
    booksim_config['traffic'] = bs_traffic
    booksim_config['trace_file'] = bs_trace_file
    booksim_config['sample_period'] = bs_sample_period
    # Perform BookSim simulation
    bs_results = rb.run_booksim(booksim_config, nw.adj_list, nw.latencies, nw.node_counts, nw.node_latencies, exp.name, verbose)
    # Prepare configuration for Orion
    orion_config = {}
    orion_config["n_vcs"] = exp.num_vcs
    orion_config["vc_buf_size"] = exp.vc_buf_size
    orion_config["frequency"] = exp.frequency
    orion_config["data_width"] = exp.data_width
    # Run Orion power/area simulation
    o_results = ro.run_orion(orion_config, nw.adj_list)
    # Compute the link power based on the BookSim results (it reads that link utilization statistics file)
    # Differentiate between traffic mode and trace mode
    link_power = 0.0
    if booksim_config['mode'] == 'traffic':
        # Since the link utilization stats are overwritten in each repetition, we need to use the last repetition's results.
        load = bs_results[-1]["summary"]["saturation_throughput"]
        cycles = booksim_config["sample_period"]
        digits = int(-math.log10(booksim_config["precision"]))
    elif booksim_config['mode'] == 'trace':
        # To load is only needed to identify the correct link utilization stats file. For trace mode, this is always 1.0
        load = 1.0
        # Since the link utilization stats are overwritten in each repetition, we need to use the last repetition's results.
        # Again, make this work even if BookSim failed (e.g. deadlock)
        if "total_run_time_cycles" in bs_results[-1]["details"]["trace"]:
            cycles = bs_results[-1]["details"]["trace"]["total_run_time_cycles"]
        else:
            cycles = float('nan')
        digits = int(-math.log10(booksim_config["precision"]))
    elif booksim_config['mode'] == 'debug':
        load = float('nan')
        cycles = float('nan')
        digits = int(-math.log10(booksim_config["precision"]))
    else:
        hlp.print_error(f"Unknown BookSim mode: {booksim_config['mode']}")
        sys.exit(1)
    # Only perform the energy and power computations of the BookSim simulation was successful
    if not np.isnan(load) and not np.isnan(cycles):
        # Compute link power
        link_power = clp.compute_link_power(nw.adj_list, nw.lengths, exp.name, load, exp.frequency, cycles, exp.packet_size, exp.data_width, digits)
        # Use the saturation throughput from BookSim, the Router power from Orion, and the computed link power to get energy per packet.
        # NOTE: The energy per packet is compute at the saturation throughput of a given configuration.
        energy_per_second = o_results["summary"]["total_router_power"] + link_power                    # in Watts = Joules/second
        energy_per_cycle = energy_per_second / exp.frequency                                           # in Joules/cycle
        packets_per_cycle = load / exp.packet_size * sum(nw.node_counts)                                # in packets/cycle
        energy_per_packet = energy_per_cycle / packets_per_cycle * 1e9                                  # in nJoules/packet
    # Just set to NaN if BookSim failed
    else:
        link_power = float('nan')
        energy_per_packet = float('nan')
    # Compose results
    # We perform repeated BookSim runs with different random seeds, but we only run Orion once since it is deterministic.
    # For simplicity, we just attach the same Orion results to each BookSim result.
    results = []
    for bs_result in bs_results:
        result = {}
        # Merge BookSim summary and Orion summary
        summary = {"status" : "success"}
        summary.update(bs_result["summary"])
        summary.update(o_results["summary"])
        summary["total_link_power"] = link_power
        summary["energy_per_packet"] = energy_per_packet
        result["summary"] = summary
        result["orion_details"] = o_results["details"]
        result["orion_config"] = orion_config
        result["experiment_config"] = {key : value for key, value in experiment.items() if type(value) in [int, float, str, bool]}
        result["booksim_details"] = bs_result["details"]
        result["booksim_config"] = booksim_config
        results.append(result)
    # Store results
    if not debug_mode:
        hlp.write_json(filename, results)
    return

def run_experiments(experiments : list[dict], multithreading : bool = False, verbose : bool = False) -> None:
    if multithreading:
        max_workers = os.cpu_count()  # hardware concurrency
        with ThreadPoolExecutor(max_workers=max_workers) as ex:
            ex.map(lambda e: run_single_experiment(e, verbose), experiments)
    else:
        for experiment in experiments:
            run_single_experiment(experiment, verbose)


