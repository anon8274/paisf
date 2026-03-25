# Python modules
import sys
import copy

# Custom modules
import helpers as hlp
import experiment_vary_topo_noc as evt_noc
import experiment_vary_topo_pici as evt_pici
import experiment_vary_topo_aici as evt_acic
import experiment_vary_topo_wsi as evt_wsi

other_sweeps = {
        'noc' : evt_noc,
        'pici' : evt_pici,
        'aici' : evt_acic,
        'wsi' : evt_wsi,
        }

# Common parameters
common_parameters = {
    'nodes_per_router': 1,                                                  # Number of nodes connected to each router
    'node_link_latency': 5,                                                 # Latency from nod to router: 3 cycles
    "num_vcs": 4,                                                           # Number of virtual channels, we use 4 as a common default for NoCs
    "packet_size": 1,                                                       # Number of flits per packet
    "message_size" : 16,                                                    # Number of packets per message
    "frequency" : 1e9,                                                      # 1 GHz, common in NoCs
    "link_fault_rate" : 0.0,                                                # Probability of link fault
    "router_fault_rate" : 0.0,                                              # Probability of router fault
    "n_entropy_bits" : 8,                                                   # Number of entropy bits hash-based routing 
    "traffic" : "trace_atlahs_llama7B-64",                                  # GPU trace for Llama 7B with 64 GPUs
    "in_order_ratio" : 1.0,                                                 # Fraction of in-order packets
    "n_routers" : 64,                                                       # Number of routers fixed to 64 for traces
}

experiments = []

# This code is automatically run when this file is imported
# It composes all experiments of this experiment sweep

# SETTING
for setting in ["noc","pici","aici","wsi"]:
    topologies = {
            "noc" : ["mesh","torus","folded_torus","hypercube","flattened_butterfly"],
            "pici" : ["mesh","folded_torus"],
            "aici" : ["mesh","double_butterfly","butterdonut","kite_small","kite_large"],
            "wsi" : ["wsi_mesh","wsi_aligned","wsi_interleaved"],
            }[setting]
    for topology in topologies:
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
                    hlp.print_error(f"Unknown routing function {routing_function} in experiment_gpu_trace.py")
                    sys.exit(1)
                # SELECTION FUNCTION
                for mod_selection_function in mod_selection_functions:
                    # Experiment Settings
                    exp = copy.deepcopy(common_parameters)
                    exp['setting'] = setting
                    # Config that is based on setting
                    exp['link_latency_function'] = other_sweeps[setting].common_parameters['link_latency_function']
                    exp['width'] = other_sweeps[setting].common_parameters['width']
                    exp['height'] = other_sweeps[setting].common_parameters['height']
                    exp['vc_buf_size'] = other_sweeps[setting].common_parameters['vc_buf_size']
                    exp['data_width'] = other_sweeps[setting].common_parameters['data_width']
                    # Varying parameters
                    exp['topology'] = topology
                    exp['routing_function'] = routing_function
                    exp['mod_routing_function'] = mod_routing_function
                    exp['mod_selection_function'] = mod_selection_function
                    # NOTE: It must be manually ensured that there are no conflicts in experiment names across different experiment sweep files and experiments within a sweep
                    #       If this is not the case, results may be overwritten and lost!
                    exp['name'] = f"gpu_trace_{setting}_{topology}_{routing_function}_{mod_routing_function}_{mod_selection_function}"
                    # NOTE: The list of varied parameters must match the order in which they appear in the experiment name (see above)
                    exp['vary'] = ["setting","topology","routing_function","mod_routing_function","mod_selection_function"]
                    # Settings for plotting, the order in these lists determines the arrangement in the heatmap grid
                    exp['plot_type'] = 'heatmap_trace'
                    exp['plot_cols'] = ['setting','topology']
                    exp['plot_rows'] = ['routing_function','mod_selection_function','mod_routing_function']
                    experiments.append(exp)
