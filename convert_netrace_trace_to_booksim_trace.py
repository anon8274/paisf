# Python modules
import sys
import re
import math
from typing import List, Dict, Set, Tuple
from collections import deque
from pyroaring import BitMap
import time
from collections import defaultdict

# Custom modules
import helpers as hlp
import config as cfg
from experiment_vary_topo_wsi import common_parameters as wsi_params # GOAL traces are just used for WSI experiments

def convert_netrace_trace_to_booksim_trace(path_netrace_trace : str, path_booksim_trace : str) -> None:
    """
    Converts a netrace trace file to a BookSim trace file.

    Parameters:
    path_netrace_trace (str): The file path to the input netrace trace.
    path_booksim_trace (str): The file path to the output BookSim trace.
    """
    start_time = time.time()
    instructions = []               # Instructions in the trace

    # Read the netrace trace
    nt_trace = hlp.read_json(path_netrace_trace)["packets"]


    # Compute the number of (forward) dependencies for each message
    for msg in nt_trace:
        msg["num_deps"] = 0
    for msg in nt_trace:
        for dep in msg["reverse_dependencies"]:
            if dep < len(nt_trace):
                nt_trace[dep]["num_deps"] += 1

    # Parse the trace into BookSim format
    for nt_msg in nt_trace:
        bs_msg = {}
        bs_msg["id"] = nt_msg["id"]
        bs_msg["cycle"] = nt_msg["cycle"]
        bs_msg["src"] = nt_msg["src"]
        bs_msg["dst"] = nt_msg["dst"]
        # This is actually the number of packets not the number of flits.
        # We use 9 packets for data messages and 1 packet for control messages since
        # according to the Netrace technical report, data messages are 9 times larger than control messages.
        bs_msg["num_flits"] = (9 if nt_msg["type"] in [2,3,4,6,16,30] else 1)
        bs_msg["rev_deps"] = nt_msg["reverse_dependencies"]
        bs_msg["num_deps"] = nt_msg["num_deps"]
        bs_msg["duration"] = 0 # This is used to model computation, but we do not have that information in the netrace trace.
        bs_msg["ignore"] = False # Ignore is used to handle receive events, which are not present in the netrace trace.
        instructions.append(bs_msg)

    print("INFO: Instructions generated in %.2f seconds" % (time.time() - start_time))
    print("INFO: Total number of instructions: %s" % len(instructions))

    # Store the BookSim trace
    hlp.print_cyan("Writing BookSim trace to %s" % path_booksim_trace)
    hlp.write_json(path_booksim_trace, instructions)

if __name__ == "__main__":
    if len(hlp.sys.argv) != 3:
        hlp.print_error("Usage: python convert_netrace_trace_to_booksim_trace.py <path_netrace_trace> <path_booksim_trace>")
        hlp.sys.exit(1)
    path_netrace_trace = hlp.sys.argv[1]
    path_booksim_trace = hlp.sys.argv[2]
    convert_netrace_trace_to_booksim_trace(path_netrace_trace, path_booksim_trace)
