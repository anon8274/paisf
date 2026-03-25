# Python modules
import sys
import copy

# Custom modules
import helpers as hlp
import experiment_vary_topo_noc as evt_noc
import experiment_vary_topo_pici as evt_pici
import experiment_vary_topo_aici as evt_acic
import experiment_vary_topo_wsi as evt_wsi
import run_experiments as re

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
    "traffic" : "debug",                                                  # 
    "in_order_ratio" : 1.0,                                                 # Fraction of in-order packets
    "routing_function" : "modular_routing",                                                   
    "mod_selection_function" : "deterministic",
}

experiments = []

# This code is automatically run when this file is imported
# It composes all experiments of this experiment sweep

# MOD ROUTING FUNCTION
for mod_routing_function in ["lash","dfsssp"]:
    # SETTING
    for setting in ["noc","pici","aici","wsi"]:
        topologies = {
                "noc" : ["mesh","torus","folded_torus","hypercube","flattened_butterfly"],
                "pici" : ["mesh","folded_torus","hexamesh","foldedhexatorus"],
                "aici" : ["mesh","double_butterfly","butterdonut","kite_small","kite_medium","kite_large"],
                "wsi" : ["wsi_mesh","wsi_aligned","wsi_interleaved","wsi_rotated"],
                }[setting]
        for topology in topologies:
            n_routers = 61 if "hexa" in topology else (66 if topology == "wsi_rotated" else 64)
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
            exp['mod_routing_function'] = mod_routing_function
            exp['topology'] = topology
            exp['n_routers'] = n_routers
            # NOTE: It must be manually ensured that there are no conflicts in experiment names across different experiment sweep files and experiments within a sweep
            #       If this is not the case, results may be overwritten and lost!
            exp['name'] = f"vc_req_{mod_routing_function}_{setting}_{topology}"
            # NOTE: The list of varied parameters must match the order in which they appear in the experiment name (see above)
            exp['vary'] = ["mod_routing_function","setting","topology"]
            experiments.append(exp)

if __name__ == "__main__":
    re.run_experiments(experiments, multithreading=False, verbose=False)
