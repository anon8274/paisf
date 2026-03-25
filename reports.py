# Import python modules
import os
import re
import argparse
import math
from types import SimpleNamespace
import numpy as np


# Import custom modules
import config as cfg
import helpers as hlp
import generate_network as gn
import explore_vc_req
from main import experiment_sweeps


def create_booksim_execution_report(experiments : dict[list[dict]], name : str) -> None:
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments, suppress_output = True)
    hlp.print_cyan("Creating BookSim execution overview for \"%s\"" % name)
    print("Legend: ", hlp.green("Success"), ",", hlp.red("Unstable simulation (saturation reached)"), ",", hlp.yellow("All %d attempts deadlocked" % cfg.default_booksim_config["deadlock_attempts"]), ",", hlp.magenta("Time limit exceeded"), ",", hlp.cyan("Other failures"))
    print("DL: Number of deadlocked runs (likely due to message-level deadlocks)")
    print("CDG: Channel Dependency Graph Checks => M: Missing CDG check, C: Cyclic CDG detected")
    max_exp_name_length = max([len(experiment["name"]) for experiment in experiments])
    for (i, experiment) in enumerate(experiments):
        # Skip missing results
        if experiment["name"] not in all_results:
            continue
        # Get results
        results = all_results[experiment["name"]]
        if results is None:
            continue
        results = results[0] # TODO: Average over all repetitions
        loads_str = [load_str for load_str in results["booksim_details"].keys() if load_str != "trace"]
        loads_float = sorted([float(load_str) for load_str in loads_str])
        print(f"Experiment: {experiment['name']}", end = "")
        print(" " * (max_exp_name_length - len(experiment['name']) + 2), end = "")
        total_deadlocks = sum([results["booksim_details"][str(load_float)]["number_of_deadlocked_runs"] if "number_of_deadlocked_runs" in results["booksim_details"][str(load_float)] else 0 for load_float in loads_float])
        total_missing_cdg = sum([1 if "cdg_check" not in results["booksim_details"][str(load_float)] or results["booksim_details"][str(load_float)]["cdg_check"] == "unknown" else 0 for load_float in loads_float])
        total_cyclic_cdg = sum([1 if "ERROR" in results["booksim_details"][str(load_float)]["cdg_check"] else 0 for load_float in loads_float if "cdg_check" in results["booksim_details"][str(load_float)]])
        total_ooo_packets = sum([results["booksim_details"][str(load_float)]["total_ooo_packets"] if "total_ooo_packets" in results["booksim_details"][str(load_float)] else 0 for load_float in loads_float])
        dl_txt = ("[DL: %d] " % total_deadlocks) + (" " * (2 - len(str(total_deadlocks))))
        hlp.print_yellow(dl_txt, end = "") if total_deadlocks > 0 else hlp.print_green(dl_txt, end = "")
        cdg_txt = ("[CDG: %d M / %d C] " % (total_missing_cdg, total_cyclic_cdg)) + (" " * (3 - len(str(total_missing_cdg)) - len(str(total_cyclic_cdg))))
        hlp.print_yellow(cdg_txt, end = "") if total_missing_cdg > 0 or total_cyclic_cdg > 0 else hlp.print_green(cdg_txt, end = "")
        ooo_txt = "[OOO: %d] " % total_ooo_packets
        hlp.print_yellow(ooo_txt, end = "") if total_ooo_packets > 0 else hlp.print_green(ooo_txt, end = "")
        for load_float in loads_float:
            ndl = str(results["booksim_details"][str(load_float)]["number_of_deadlocked_runs"]) if "number_of_deadlocked_runs" in results["booksim_details"][str(load_float)] else "?"
            if results["booksim_details"][str(load_float)]["status"] == "Success":
                hlp.print_green(str(round(load_float, -1 * int(math.log10(cfg.default_booksim_config["precision"])))) + " (" + ndl + ") | ", end = "")
            elif results["booksim_details"][str(load_float)]["status"] == "Unstable simulation":
                hlp.print_red(str(round(load_float, -1 * int(math.log10(cfg.default_booksim_config["precision"])))) + " (" + ndl + ") | ", end = "")
            elif results["booksim_details"][str(load_float)]["status"] == "Potential Deadlock":
                hlp.print_yellow(str(round(load_float, -1 * int(math.log10(cfg.default_booksim_config["precision"])))) + " (" + ndl + ") | ", end = "")
            elif results["booksim_details"][str(load_float)]["status"] == "Time limit exceeded":
                hlp.print_magenta(str(round(load_float, -1 * int(math.log10(cfg.default_booksim_config["precision"])))) + " (" + ndl + ") | ", end = "")
            else:
                hlp.print_cyan(str(round(load_float, -1 * int(math.log10(cfg.default_booksim_config["precision"])))) + " (" + ndl + ") | ", end = "")
        print()

def create_confidence_interval_report(experiments : dict[list[dict]], name : str) -> None:
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments, suppress_output = True)
    hlp.print_cyan("Creating confidence interval report for \"%s\"" % name)
    overall_rel_hw_cis = {}
    for (i, experiment) in enumerate(experiments):
        # Skip missing results
        if experiment["name"] not in all_results:
            continue
        # Get results
        results = all_results[experiment["name"]]
        if results is None:
            continue
        # Iterate through metrics
        if "trace" in experiment["name"]:
            metrics = ["average_latency", "average_throughput","energy_per_packet"]
        else:
            metrics = ["saturation_throughput", "zero_load_latency", "energy_per_packet"]
        for metric in metrics:
            if metric in ["average_latency", "zero_load_latency"]:
                mean_func = hlp.avg_arithmetic
            elif metric in ["average_throughput", "saturation_throughput", "energy_per_packet"]:
                mean_func = hlp.avg_harmonic
            values = [results[rep]["summary"][metric] for rep in range(len(results)) if not math.isnan(results[rep]["summary"][metric])]
            # Skip if no valid values (i.e. all runs deadlocked or failed)
            if len(values) == 0:
                continue
            mean, lower, upper, rel_half_width = hlp.compute_confidence_interval(values, mean_func)
            if metric not in overall_rel_hw_cis:
                overall_rel_hw_cis[metric] = []
            overall_rel_hw_cis[metric].append(rel_half_width)
    hlp.print_magenta("Overall mean relative half-widths of 95% confidence intervals for experiment " + name + ":")
    for metric in overall_rel_hw_cis:
        hlp.print_cyan(f"  {metric}: {np.mean(overall_rel_hw_cis[metric]):.2%}")
    return overall_rel_hw_cis

def create_vc_req_report(explorations : dict[list[dict]], name : str) -> None:
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments, suppress_output = True)
    hlp.print_cyan("Creating VC requirement report for \"%s\"" % name)
    rf_to_vc_map = {}
    print(f"{'Routing Function':<20} {'Topology':<30} {'Required VC layers':<20} {'Assigned VC layers':<20} {'Status':<10}")
    for (i, experiment) in enumerate(experiments):
        # Skip missing results
        if experiment["name"] not in all_results:
            continue
        # Get results
        results = all_results[experiment["name"]]
        if results is None:
            continue
        rf = experiment["mod_routing_function"]
        if rf not in rf_to_vc_map:
            rf_to_vc_map[rf] = 0
        # Get the booksim output stored in debug mode
        bs_out = results[0]["summary"]["info"]
        m = re.search(r"requires (\d+) layers, (\d+) were assigned", bs_out)
        if m is not None:
            required_layers, assigned_layers = map(int, m.groups())
            status = hlp.green("OK") if assigned_layers >= required_layers else hlp.red("ERROR")
            rf_to_vc_map[rf] = max(rf_to_vc_map[rf], required_layers)
            print(f"{experiment['mod_routing_function']:<20} {experiment['topology']:<30} {required_layers:<20} {assigned_layers:<20} {status:<10}")
        else:
            status = hlp.yellow("Unknown")
            print(f"{experiment['mod_routing_function']:<20} {experiment['topology']:<30} {'?':<20} {'?':<20} {status:<10}")
    print("\nSummary of maximum required VC layers per routing function across all topologies:")
    for rf in rf_to_vc_map:
        hlp.print_cyan(f"  {rf}: {rf_to_vc_map[rf]} layers")

def create_routing_function_report(experiments : dict[list[dict]], name : str) -> None:
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments, suppress_output = True)
    hlp.print_cyan("Creating routing function report for \"%s\"" % name)
    rf_to_metric_map = {}
    metric_name = "<metric>"
    for (i, experiment) in enumerate(experiments):
        # Skip missing results
        if experiment["name"] not in all_results:
            continue
        # Get results
        results = all_results[experiment["name"]]
        if results is None:
            continue
        rf = experiment["mod_routing_function"]
        if rf not in rf_to_metric_map:
            rf_to_metric_map[rf] = []
        # Get the booksim output stored in debug mode
        if "trace" in experiment["traffic"]:
            metric = hlp.avg_harmonic([results[rep]["summary"]["average_latency"] for rep in range(len(results)) if not math.isnan(results[rep]["summary"]["average_latency"])])
            metric_name = "average latency"
        else:
            metric = hlp.avg_harmonic([results[rep]["summary"]["saturation_throughput"] for rep in range(len(results))])
            metric_name = "saturation throughput"
        if not math.isnan(metric):
            rf_to_metric_map[rf].append(metric)
    # Sort results best to worst based on average throughput across all experiments
    rf_to_metric_map = dict(sorted(rf_to_metric_map.items(), key = lambda item: np.mean(item[1]), reverse = True))
    print(f"Summary of average {metric_name} per routing function across all topologies and traffic patterns for " + name + ":")
    for rf in rf_to_metric_map:
        hlp.print_cyan(f"  {rf}: {np.mean(rf_to_metric_map[rf]):.4f} flits/cycle")



if __name__ == "__main__":
    # Read command line arguments (-b --booksim for BookSim execution overview, -c --ci for confidence interval report)
    parser = argparse.ArgumentParser(description = "Generate reports for BookSim experiments")
    parser.add_argument("-b", "--booksim", action = "store_true", help = "Generate BookSim execution overview")
    parser.add_argument("-c", "--ci", action = "store_true", help = "Generate confidence interval report")
    parser.add_argument("-v", "--vc_req", action = "store_true", help = "Generate VC requirement report")
    parser.add_argument("-r", "--routing_function", action = "store_true", help = "Generate routing function performance report")
    args = parser.parse_args()

    # Aggregator for overall mean relative half-widths of confidence intervals across all experiments
    overall_rel_hw_cis = {}

    # Reports for experiment sweeps
    for exp_name, experiments in experiment_sweeps.items():
        # Create BookSim execution overview plots
        if args.booksim:
            create_booksim_execution_report(experiments, exp_name)
        if args.ci:
            rel_hw_cis = create_confidence_interval_report(experiments, exp_name)
            for metric in rel_hw_cis:
                if metric not in overall_rel_hw_cis:
                    overall_rel_hw_cis[metric] = []
                overall_rel_hw_cis[metric].extend(rel_hw_cis[metric])
        if args.routing_function:
            create_routing_function_report(experiments, exp_name)
    # Special report for VC requirements (only for experiments with debug info) 
    if args.vc_req:
        experiments = explore_vc_req.experiments
        print(experiments[0]["name"])
        create_vc_req_report(explore_vc_req.experiments, "vc_req_lash")

    # Report overall mean relative half-widths of confidence intervals across all experiments
    if args.ci:
        hlp.print_green("Overall mean relative half-widths of 95% confidence intervals across all experiments:")
        for metric in overall_rel_hw_cis:
            hlp.print_cyan(f"  {metric}: {np.mean(overall_rel_hw_cis[metric]):.2%}")
        

