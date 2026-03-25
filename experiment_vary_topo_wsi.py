# Python modules
import sys
import math
import copy

# Custom modules
import helpers as hlp

# Common parameters
common_parameters = {
    'nodes_per_router': 1,                                                  # Number of nodes connected to each router
    'link_latency_function': lambda length : int(math.ceil(0.5 * length)),  # Latency: 1 cycle per 2 mm
    'node_link_latency': 5,                                                 # Latency from nod to router: 3 cycles
    'width': 26,                                                            # Width of unit (router not node) in hardware (reticle limits of 26mm x 33mm)
    'height': 33,                                                           # Height of unit (router not node) in hardware (reticle limits of 26mm x 33mm)
    "num_vcs": 4,                                                           # Number of virtual channels, we use 4 as a common default for NoCs
    "vc_buf_size": 32,                                                       # Buffer depth, we use 8 flits as a common default for NoCs
    "packet_size": 4,                                                       # Number of flits per packet
    "message_size" : 16,                                                    # Number of packets per message
    "in_order_ratio" : 0.5,                                                 # Ratio of messages that require in-order delivery
    "frequency" : 1e9,                                                      # 1 GHz, common in NoCs
    "data_width" : 16000,                                                   # Results in 2TB/s per link at 1 GHz (like Tesla DOJO and others)
    "link_fault_rate" : 0.0,                                                # Probability of link fault
    "router_fault_rate" : 0.0,                                              # Probability of router fault
    "n_entropy_bits" : 8,                                                   # Number of entropy bits hash-based routing 
}

experiments = []

# This code is automatically run when this file is imported
# It composes all experiments of this experiment sweep

# TRAFFIC
for traffic in ["uniform","randperm","asymmetric"]:
    # TOPOLOGY
    for topology in ["wsi_mesh","wsi_aligned","wsi_interleaved","wsi_rotated"]:
        # N is usually set based on the topology, in this experiment sweep, all topologies work with 64 routers
        n_routers = 66 if topology == "wsi_rotated" else 64
        # MULTI yes or no
        for routing_function in ["modular_routing","multi_routing"]:
            mod_routing_functions = ["left_right","up_down","prefix","simple_cycle_breaking_set","lturn"]              
            # For mesh, we also test xy routing, for other topologies, it doesn't make sense
            if topology == "mesh":
                mod_routing_functions.append("xy")
            if routing_function == "modular_routing":
                mod_routing_functions.append("lash_tor")
            # ROUTING FUNCTION
            for mod_routing_function in mod_routing_functions:
                # With modular routing, we need to guarantee in-order delivery, hence, we use hashed and adaptive_hashed only
                if routing_function == "modular_routing":
                    mod_selection_functions = ["deterministic"]
                    if mod_routing_function not in ["lash_tor"]:
                        mod_selection_functions += ["hashed","adaptive_hashed"]
                # With multi routing, we can use arbitrary selection functions. 
                # It does not make sense to use hashed and adaptive_hashed because this would enforce in-order delivery twice
                elif routing_function == "multi_routing":
                    mod_selection_functions = ["random","adaptive"]
                else:
                    hlp.print_error(f"Unknown routing function {routing_function} in experiment_vary_topo_wsi.py")
                    sys.exit(1)
                # SELECTION FUNCTION
                for mod_selection_function in mod_selection_functions:
                    # Experiment Settings
                    exp = copy.deepcopy(common_parameters)
                    exp['traffic'] = traffic
                    exp['topology'] = topology
                    exp['n_routers'] = n_routers
                    exp['routing_function'] = routing_function
                    exp['mod_routing_function'] = mod_routing_function
                    exp['mod_selection_function'] = mod_selection_function
                    # NOTE: It must be manually ensured that there are no conflicts in experiment names across different experiment sweep files and experiments within a sweep
                    #       If this is not the case, results may be overwritten and lost!
                    exp['name'] = f"vary_topo_wsi_{traffic}_{topology}_{routing_function}_{mod_routing_function}_{mod_selection_function}"
                    # NOTE: The list of varied parameters must match the order in which they appear in the experiment name (see above)
                    exp['vary'] = ["traffic","topology","routing_function","mod_routing_function","mod_selection_function"]
                    # Settings for plotting, the order in these lists determines the arrangement in the heatmap grid
                    exp['plot_type'] = 'heatmap'
                    exp['plot_cols'] = ['topology','traffic']
                    exp['plot_rows'] = ['routing_function','mod_selection_function','mod_routing_function']
                    experiments.append(exp)
