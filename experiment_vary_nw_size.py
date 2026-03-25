# Python modules
import sys
import copy

# Custom modules
import helpers as hlp
import experiment_vary_topo_noc as evt_noc
import experiment_vary_topo_aici as evt_acic

other_sweeps = {
        'noc' : evt_noc,
        'aici' : evt_acic,
        }

# Common parameters
common_parameters = {
    'nodes_per_router': 1,                                                  # Number of nodes connected to each router
    'node_link_latency': 5,                                                 # Latency from nod to router: 3 cycles
    "packet_size": 4,                                                       # Number of flits per packet
    "message_size" : 16,                                                    # Number of packets per message
    "frequency" : 1e9,                                                      # 1 GHz, common in NoCs
    "link_fault_rate" : 0.0,                                                # Probability of link fault
    "router_fault_rate" : 0.0,                                              # Probability of router fault
    "n_entropy_bits" : 8,                                                   # Number of entropy bits hash-based routing 
    "traffic" : "uniform",                                                  # Uniform is the most general traffic pattern
    "in_order_ratio" : 0.5,                                                 # Fraction of in-order messages
    "message_size" : 16,                                                    # Number of packets per message
    "num_vcs" : 4,                                                          # Number of virtual channels per physical channel
}

experiments = []

# This code is automatically run when this file is imported
# It composes all experiments of this experiment sweep

# SETTING
for setting in ["noc","aici"]:
    topology = {"noc" : "mesh", "aici" : "kite_large"}[setting]
    for n_routers in [4,16,64,256]:
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
                    hlp.print_error(f"Unknown routing function {routing_function} in experiment_vary_num_vcs.py")
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
                    exp['n_routers'] = n_routers
                    exp['routing_function'] = routing_function
                    exp['mod_routing_function'] = mod_routing_function
                    exp['mod_selection_function'] = mod_selection_function
                    # NOTE: It must be manually ensured that there are no conflicts in experiment names across different experiment sweep files and experiments within a sweep
                    #       If this is not the case, results may be overwritten and lost!
                    exp['name'] = f"vary_nw_size_{setting}_{n_routers}_{routing_function}_{mod_routing_function}_{mod_selection_function}"
                    # NOTE: The list of varied parameters must match the order in which they appear in the experiment name (see above)
                    exp['vary'] = ["setting","n_routers","routing_function","mod_routing_function","mod_selection_function"]
                    # Settings for plotting, the order in these lists determines the arrangement in the heatmap grid
                    exp['plot_type'] = 'lines'
                    exp['plot_subplot'] = 'setting'
                    exp['plot_x_axis'] = 'n_routers'
                    exp['plot_lines'] = ['routing_function','mod_selection_function','mod_routing_function']
                    experiments.append(exp)
