# Import python modules 
import os
import sys
import math
import time
import copy
import subprocess

# Import custom modules
import helpers as hlp

# Define return codes of our modified BookSim version
return_codes = {
    0: "Success",
    1: "Potential Deadlock",
    2: "Unstable simulation",
    3: "Time limit exceeded"
}

# Check whether the CDG checker was enabled and return its result
def check_cdg(out_string : str) -> str:
    lines = out_string.split('\n')
    cdg_check = "unknown"
    for line in lines:
        if "CDG CHECK" in line:
            cdg_check = line
            break
    return cdg_check

# Read the BookSim results
# - out = stdout string of BookSim execution
# RETURNS: dictionary containing the results
def read_booksim_results(out : str) -> dict:
    result_lines = out.split("\n")[-35:]
    metrics = ["Packet latency","Network latency","Flit latency","Fragmentation","Injected packet rate","Accepted packet rate","Injected flit rate","Accepted flit rate"]
    results = {}
    for (line_idx, line) in enumerate(result_lines):
        for metric in metrics:
            if (metric + " average") in line:
                key = metric.lower().replace(" ","_")
                key_length = len(key.split("_"))    
                results[key] = {}
                results[key]["avg"] = float(result_lines[line_idx].split(" ")[key_length + 2]) if len(result_lines[line_idx].split(" ")) > key_length + 2 else float("nan")
                results[key]["min"] = float(result_lines[line_idx+1].split(" ")[2]) if len(result_lines[line_idx+1].split(" ")) > 2 else float("nan")
                results[key]["max"] = float(result_lines[line_idx+2].split(" ")[2])    if len(result_lines[line_idx+2].split(" ")) > 2 else float("nan")
        if "Injected packet size average" in line:
            results["injected_packet_size"] = {}
            results["injected_packet_size"]["avg"] = float(line.split(" ")[5])
        if "Accepted packet size average" in line:
            results["accepted_packet_size"] = {}
            results["accepted_packet_size"]["avg"] = float(line.split(" ")[5])
        if "Hops average" in line:
            results["hops"] = {}
            results["hops"]["avg"] = float(line.split(" ")[3])
        if "Total run time" in line:
            results["total_run_time"] = float(line.split(" ")[3])
        if "Total cycles until trace completion" in line:
            results["total_run_time_cycles"] = float(line.split(" ")[6])
        if "Truly simulated cycles" in line:
            results["truly_simulated_cycles"] = float(line.split(" ")[4])
        if "Total number of trace messages simulated" in line:
            results["total_trace_messages"] = int(line.split(" ")[7])
        if "Total number of trace instructions simulated" in line:
            results["total_trace_instructions"] = int(line.split(" ")[7])
        if "Total number of out-of-order packets" in line:
            results["total_ooo_packets"] = int(line.split(" ")[6])
        if "Peak RoB size" in line:
            results["peak_rob_size"] = int(line.split(" ")[4])
        if "Peak number of RoBs per node" in line:
            results["peak_robs_per_node"] = int(line.split(" ")[7])
        if "Selection function storage overhead" in line:
            results["selection_function_storage_overhead"] = int(line.split(" ")[5])
    results["last_ten_lines_of_output"] = "\n".join(result_lines[-10:])
    return results

# Perform one simulation run of BookSim and return the results
# - exec_path = path to the BookSim executable
# - config_path = path to the BookSim configuration file
# - log_path = path to the directory where log files should be stored
# - time_limit = time limit for the simulation in seconds
# - exec_identifier = unique identifier for this execution (used for log file names)
# - verbose = whether to print detailed output to the console
# RETURNS: dictionary containing the results of the simulation
def execute_booksim(exec_path : str, config_path : str, log_path : str, time_limit : int, exec_identifier : str, verbose : bool = False) -> dict:
    # Execute BookSim
    start_time = time.monotonic()
    proc = subprocess.Popen([exec_path, config_path],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    result = {}
    ret = 0
    out = ""
    err = ""
    write_log = False
    # Try to read the output within the time limit
    try:
        out, err = proc.communicate(timeout = time_limit)
        ret = proc.returncode
        runtime = time.monotonic() - start_time
        hlp.print_green("BookSim execution of \"%s\" completed in %.3f seconds -> %s" % (exec_identifier, runtime, return_codes.get(ret, "Unknown return code %d" % ret))) if verbose else None
        result["return_code"] = ret
        result["status"] = return_codes.get(ret, "Unknown return code %d" % ret)
        result["runtime"] = runtime
        result["cdg_check"] = check_cdg(out)
        result.update(read_booksim_results(out))
        write_log = (ret not in [0,2])  # Write log files only if the simulation failed (deadlock or other error)
    # If the time limit is exceeded, kill the process and store partial output
    except subprocess.TimeoutExpired:
        proc.kill()
        hlp.print_red("BookSim execution of %s exceeded time limit of %ds and was killed" % (exec_identifier, time_limit)) if verbose else None
        out, err = proc.communicate()
        runtime = time.monotonic() - start_time
        ret = 4
        result["return_code"] = ret
        result["status"] = "Time limit of %ds exceeded" % time_limit
        result["runtime"] = runtime
        result["cdg_check"] = "unknown"
        write_log = True
    # Store the output and error messages in log files if the simulation failed
    if write_log:
        # Create log directory if it does not exist yet
        if not os.path.exists(log_path):
            os.makedirs(log_path)
        # Prepare log files
        stdout_file = log_path + "/" + exec_identifier + "_stdout.log"
        stderr_file = log_path + "/" + exec_identifier + "_stderr.log"
        # Store logs
        with open(stdout_file, "w") as file:
            file.write(out)
        with open(stderr_file, "w") as file:
            file.write(err)
    # Print errors to console if the simulation failed (deadlock or time limit) but do not print anything if successful or unstable
    if verbose and ret not in [0,2]:
        hlp.print("===== Start of BookSim output =====")
        hlp.print("Return code: %d (%s)" % (ret, return_codes.get(ret, "Unknown return code %d" % ret)))
        hlp.print("----- Std Out -----")
        hlp.print("\n".join(out.splitlines()[-50:]))
        hlp.print("----- Std Err -----")
        hlp.print("\n".join(err.splitlines()[-50:]))
        hlp.print("===== End of BookSim output =====")
    # Return the results
    return result

# Export the BookSim configuration file
# - booksim_config = dictionary containing the BookSim configuration parameters
# - run_identifier = unique identifier for this experiment
# - load = injection rate to be used for this run (between 0.0 and 1.0)
# RETURNS: None
def export_booksim_config(booksim_config : dict, run_identifier : str, load : float) -> None:
    # Prepare the BookSim configuration file for export
    bsc = copy.deepcopy(booksim_config)
    router_latency = bsc["router_latency"]
    # Remove parameters that are used by this wrapper but not by BookSim directly
    del bsc["time_limit"]
    del bsc["precision"]
    del bsc["saturation_factor"]
    del bsc["router_latency"]
    del bsc["deadlock_attempts"]
    # 1) Simulation parameters
    bsc["topology"] = "anynet" 
    bsc["network_file"] = "booksim2/src/topologies/%s.anynet" % run_identifier
    bsc["injection_rate"] = 1.0 if bsc["mode"] == "trace" else load
    # 3) Parameters related to the timing/latencies:
    bsc["credit_delay "] = 0
    bsc["routing_delay "] = 1
    bsc["vc_alloc_delay "] = 1
    bsc["sw_alloc_delay "] = 1
    bsc["st_final_delay "] = max(1, router_latency - 3)
    bsc["input_speedup "] = 1
    bsc["output_speedup "] = 1
    bsc["internal_speedup "] = (1.0 if router_latency >= 3 else (3.0 / router_latency))
    # 4) More simulation parameters
    bsc["use_read_write "] = 0
    bsc["path_for_stats"] = "booksim2/src/stats/%s_%f.json" % (run_identifier, load)
    bsc["path_for_cache"] = "booksim2/src/cache/"
    # Convert configuration file to correct format
    config_lines = [(key + " = " + str(bsc[key]) + ";") for key in bsc]
    # Store the file
    save_path = "booksim2/src/configs/%s.conf" % run_identifier 
    with open(save_path, "w") as file:
        for line in config_lines:
            file.write(line + "\n")

# Export the BookSim topology file
# - adj_list[source_router_id] = [list of destination_router_ids]
# - latencies[source_router_id] = [list of latencies to destination_router_ids in cycles (int)] // Same order as adj_list
# - node_counts[router_id] = number of nodes connected to router_id
# - node_latencies[router_id][node_id] = latency of link connecting node_id to router_id in cycles (int)
# - run_identifier = unique identifier for this experiment
# RETURNS: None
def export_booksim_topology(adj_list : list[list[int]], latencies : list[list[int]], node_counts : list[int], node_latencies : list[list[int]], run_identifier : str) -> None:
    n_routers = len(adj_list)
    running_node_id_counter = 0
    topology_lines = []
    for rid in range(n_routers):
        # Write one line per router:
        line = "router % d" % rid 
        # Add nodes (if any)
        for nid in range(node_counts[rid]):
            line += " node %d %d" % (running_node_id_counter, node_latencies[rid][nid])
            running_node_id_counter += 1
        # Add links to other routers
        for (other_rid, lat)in zip(adj_list[rid], latencies[rid]):
            line += " router %d %d" % (other_rid, lat)
        # Store the line
        topology_lines.append(line)
    # Store the file
    save_path = "booksim2/src/topologies/%s.anynet" % run_identifier
    with open(save_path, "w") as file:
        for line in topology_lines:
            file.write(line + "\n")

# Run a BookSim simulation:
# This runs the C++ code which needs to be built manually by executing "make" in the "booksim2/src" directory
# - booksim_config = dictionary containing the BookSim configuration parameters
# - run_identifier = unique identifier for this experiment
# RETURNS: dictionary containing the results of the simulation
def perform_simulation_with_increasing_load(booksim_config : dict, run_identifier : str, verbose : bool = False) -> dict:
    time_limit = booksim_config["time_limit"]
    precision = booksim_config["precision"]
    deadlock_attempts = booksim_config["deadlock_attempts"]
    saturation_factor = booksim_config["saturation_factor"]
    exec_path = "booksim2/src/booksim"
    config_path = "booksim2/src/configs/%s.conf" % run_identifier
    log_path = "booksim2/src/logs"
    results = {}
    # Traffic mode: Iterate through loads
    if booksim_config["mode"] == "traffic":
        decimal_places = max(0, -int(math.log10(precision)))
        min_load = round(10**(-decimal_places),9)   # Start with the lowest load
        max_load = round(1.0 - min_load,9)          # End with the highest load
        load = min_load                             # Current load
        granularity = 0.1                           # Current granularity -> Will be reduced if saturation point is reached
        while True:
            saturation_reached = False
            deadlock_counter = 0
            # Export the BookSim configuration file
            export_booksim_config(booksim_config, run_identifier, load)
            exec_identifier = "%s-load_%0.*f" % (run_identifier, decimal_places, load)
            # Run BookSim: If we counter deadlocks, we try again with another seed. After a maximum number of attempts we give up
            for att in range(deadlock_attempts):
                result = execute_booksim(exec_path, config_path, log_path, time_limit, exec_identifier, verbose)
                if result["return_code"] != 1:   # No deadlock
                    break
                else:
                    hlp.print_yellow("WARNING: Deadlock detected in BookSim simulation \"%s\" (attempt %d of %d). Retrying with different seed..." % (exec_identifier, att + 1, deadlock_attempts)) if verbose else None
                    deadlock_counter += 1
            # We store the result of each run, also the not successful ones. It is up to the user to check the logs
            result["number_of_deadlocked_runs"] = deadlock_counter
            results[str(load)] = result
            # If the simulation was successful: Proceed with the next load
            if result["return_code"] == 0:
                # If the packet latency is missing -> Something went wrong. Abort the simulation
                if "packet_latency" not in result:
                    txt = ("ERROR: Packet latency missing in BookSim output for load %%.%df. " % decimal_places)
                    hlp.print_error(txt % load)
                    break
                # Check if saturation throughput has been reached
                elif (min_load in results) and ((results[min_load]["packet_latency"]["avg"] * saturation_factor) < results[load]["packet_latency"]["avg"]):
                    saturation_reached = True
            # We treat each unsuccessful run as saturation point reached
            # It is up to the user to check the logs and see why the run failed
            else:
                saturation_reached = True
            # If the saturation point has been reached go to finer granularity or abort
            if saturation_reached:
                # We already are at the maximum precision -> Terminate
                if granularity <= precision:
                    break
                # If we are at the minimum load but we already reached saturation -> Terminate
                elif load == min_load:
                    break
                # We can reduce granularity
                else:
                    load = round((load - granularity) + (granularity / 10),9)
                    granularity = round(granularity * 0.1,9)
            # Saturation point not reached yet
            elif load < max_load:
                if (load == min_load) and (granularity > precision):
                    load = round(granularity, 9)
                else:
                    load = round(load + granularity,9)
            # Network can support a load of 0.999 -> Terminate
            else:
                break
    elif booksim_config["mode"] == "trace":
        export_booksim_config(booksim_config, run_identifier, load = 1.0)
        # Run BookSim: If we counter deadlocks, we try again with another seed. After a maximum number of attempts we give up
        deadlock_counter = 0
        result = {}
        for att in range(deadlock_attempts):
            result = execute_booksim(exec_path, config_path, log_path, time_limit, run_identifier, verbose)
            if result["return_code"] != 1:   # No deadlock
                break
            else:
                hlp.print_yellow("WARNING: Deadlock detected in BookSim simulation \"%s\" (attempt %d of %d). Retrying with different seed..." % (run_identifier, att + 1, deadlock_attempts)) if verbose else None
                deadlock_counter += 1
        # We store the result of each run, also the not successful ones. It is up to the user to check the logs
        result["number_of_deadlocked_runs"] = deadlock_counter
        results["trace"] = result
    elif booksim_config["mode"] == "debug":
        export_booksim_config(booksim_config, run_identifier, load = 1.0)
        result = execute_booksim(exec_path, config_path, log_path, time_limit, run_identifier, verbose)
        results["debug"] = result
    else:
        hlp.print_error("ERROR: Unknown BookSim mode '%s' specified." % booksim_config["mode"])
    results = dict(sorted(results.items()))
    return results    

# Runs a full BookSim experiment including multiple repetitions with increasing load (for synthetic traffic patterns)
def run_booksim(booksim_config : dict, adj_list : list[list[int]], latencies : list[list[int]], node_counts : list[int], node_latencies : list[list[int]], run_identifier : str, verbose : bool = False) -> list[dict]:
    # Read the number of repetitions and remove it from the config
    booksim_reps = booksim_config["repetitions"]
    del booksim_config["repetitions"]
    # Export the BookSim topology (same topology for all repetitions)
    export_booksim_topology(adj_list, latencies, node_counts, node_latencies, run_identifier)
    # Repeat the BookSim simulation if needed
    last_saturation_load = 0.0
    results = []
    for rep in range(booksim_reps):
        # Store rep-id in BookSim config
        booksim_config["repetition"] = rep
        # User info
        hlp.print_cyan("Performing BookSim simulation \"%s\" repetition %d of %d..." % (run_identifier, rep + 1, booksim_reps)) if verbose else None
        # Run one repetition of the BookSim simulation
        bs_results = perform_simulation_with_increasing_load(booksim_config, run_identifier, verbose)
        # Write a summary of the results with the most important metrics
        summary = {}
        all_valid_loads = [float(load) for load in bs_results.keys() if (hlp.is_float(load) and bs_results[load]["status"] == "Success" and "packet_latency" in bs_results[load].keys())]
        has_trace_results = "trace" in bs_results.keys()
        has_debug_results = "debug" in bs_results.keys()
        # Trace-based simulation: Store average latency and throughput directly from the trace results
        if has_trace_results and len(all_valid_loads) == 0:
            # Safeguard to not crash if all BookSim runs deadlocked and there are no latency/throughput results
            if ("flit_latency" in bs_results["trace"].keys() and bs_results["trace"]["number_of_deadlocked_runs"] < booksim_config["deadlock_attempts"]):
                summary["average_latency"] = bs_results["trace"]["flit_latency"]["avg"]
            else:
                summary["average_latency"] = float("nan")
            if ("accepted_flit_rate" in bs_results["trace"].keys() and bs_results["trace"]["number_of_deadlocked_runs"] < booksim_config["deadlock_attempts"]):
                summary["average_throughput"] = bs_results["trace"]["accepted_flit_rate"]["avg"]
            else:
                summary["average_throughput"] = float("nan")
        # Debug mode to get specific BookSim output instead of results (e.g. required number of VCs for LASH or DFSSSP)
        elif has_debug_results and len(all_valid_loads) == 0:
            summary["info"] = bs_results["debug"]["last_ten_lines_of_output"]
        # This was a synthetic traffic pattern with load sweep: Store zero-load latency and saturation throughput
        elif not has_trace_results and not has_debug_results and len(all_valid_loads) > 0:
            min_load = min(all_valid_loads)
            max_load = max(all_valid_loads)
            summary["zero_load_latency"] = bs_results[str(min_load)]["packet_latency"]["avg"]
            summary["saturation_throughput"] = max_load 
            last_saturation_load = max_load
        # Unexpected case: Either load sweep and trace results simultaneously or no valid results at all
        else:
            print(booksim_config["mode"], has_trace_results, has_debug_results, len(all_valid_loads))
            hlp.print_error("ERROR: BookSim results contain load sweep results and trace results simultaneously or no valid results at all.") if verbose else None
            sys.exit(1)
        # Add the results of this repetition to the list of results
        results.append({"summary" : summary, "details" : bs_results, "config" : booksim_config})
    # Clean up all BookSim link utilization files except for the one at saturation throughput.
    if booksim_config["mode"] == "traffic":
        stats_dir = "./booksim2/src/stats/"
        files = os.listdir(stats_dir)
        digits = max(0, -int(math.log10(booksim_config["precision"])))
        for file in files:
            if (run_identifier in file) and (f"{last_saturation_load:.{digits}f}" not in file):
                os.remove(os.path.join(stats_dir, file))
    # Return the results
    return results
