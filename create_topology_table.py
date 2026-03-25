# Python modules
import sys
import copy
from types import SimpleNamespace

# Custom modules
import helpers as hlp
import config as cfg
import experiment_vary_topo_noc as evt_noc
import experiment_vary_topo_pici as evt_pici
import experiment_vary_topo_aici as evt_acic
import experiment_vary_topo_wsi as evt_wsi
import generate_network as gn

other_sweeps = {
        'noc' : evt_noc,
        'pici' : evt_pici,
        'aici' : evt_acic,
        'wsi' : evt_wsi,
        }

topologies = []



for setting in ["noc","pici","aici","wsi"]:
    sweep = other_sweeps[setting]
    topology_set = set()
    for experiment in sweep.experiments:
        exp = SimpleNamespace(**experiment)
        if exp.topology not in topology_set:
            topology_set.add(exp.topology)
            network = gn.generate_network(exp.n_routers, exp.topology, exp.nodes_per_router, exp.link_latency_function, exp.node_link_latency, exp.width, exp.height)
            nw = SimpleNamespace(**network)
            n_routers = len(nw.adj_list)
            n_endpoints = sum(nw.node_counts)
            is_direct = min(nw.node_counts) == max(nw.node_counts)
            diameter = hlp.compute_network_diameter(nw.adj_list)
            min_radix = min(len(x) for x in nw.adj_list)
            max_radix = max(len(x) for x in nw.adj_list)
            radix = str(max_radix) if min_radix == max_radix else f"{min_radix}-{max_radix}"
            topo = {"setting" : setting, "topology" : exp.topology, "n_routers" : n_routers, "n_endpoints" : n_endpoints, "is_direct" : is_direct, "diameter" : diameter, "radix" : radix}
            topologies.append(topo)


# Nicely print a table of the topologies
print(f"{'Setting':<10} & {'Topology':<30} & {'# Routers':<10} & {'# Endpoints':<12} & {'Direct':<10} & {'Diameter':<10} & {'Radix':<10} \\\\")
for topo in topologies:
    short = cfg.acronyms["topology"][topo["topology"]]
    full_name = "%s (%s)" % (topo["topology"], short)
    print(f"{topo['setting']:<10} & {full_name:<30} & {topo['n_routers']:<10} & {topo['n_endpoints']:<12} & {str(topo['is_direct']):<10} & {topo['diameter']:<10} & {topo['radix']:<10} \\\\")


