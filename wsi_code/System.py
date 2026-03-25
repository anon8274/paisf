# Python modules
from typing import List, Dict, Callable
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle

# Custom modules
from .Wafer import Wafer, create_wafer
from . import config as cfg
from . import helpers as hlp

class System:
    # Initialize a wafer-scale system
    def __init__(self, wafers : List[Wafer], integration_level : str, wafer_diameter : int, wafer_utilization : str, method : str, concentration: int):
        self.wafers = wafers
        self.integration_level = integration_level
        self.wafer_diameter = wafer_diameter
        self.wafer_utilization = wafer_utilization
        self.method = method
        self.concentration = concentration 

    # Visualize the wafer-scale system
    def visualize(self, name : str, show_global_reticle_ids : bool = False, show_reticle_attribute : str = ""):
        fig, ax = plt.subplots(1, 1 + len(self.wafers), figsize=(5 * (1 + len(self.wafers)), 5))
        # If we show the global reticle IDs, set set them as reticle attributes and we overwrite the attribute_to_show parameter
        if show_global_reticle_ids:
            if not all(["global_reticle_id" in reticle.attributes for wafer in self.wafers for reticle in wafer.reticles]):
                hlp.register_error("Not all reticles have a global_reticle_id attribute. Please run add_global_reticle_ids(system) first.")
            attribute_to_show = "global_reticle_id"
        # Visualize each wafer
        for layer, wafer in enumerate(self.wafers):
            wafer.add_to_visualization(ax = ax[0], show_local_reticle_ids = False, show_reticle_attribute = show_reticle_attribute, layer = layer)
            wafer.add_to_visualization(ax = ax[layer + 1], show_local_reticle_ids = False, show_reticle_attribute = show_reticle_attribute, layer = layer)
        # Set the title and labels
        for i in range(len(self.wafers) + 1):
            ax[i].set_title("System Overview" if i == 0 else f"Wafer {i-1}", fontsize=16)
            max_wafer_diameter = max(wafer.diameter for wafer in self.wafers)
            ax[i].set_xlim(-max_wafer_diameter/2 - 33, max_wafer_diameter/2 + 33)
            ax[i].set_ylim(-max_wafer_diameter/2 - 33, max_wafer_diameter/2 + 33)
            ax[i].set_aspect('equal', adjustable='box')
        plt.savefig("plots/" + name + "." + cfg.plot_format, dpi=cfg.plot_dpi)
        plt.close(fig)

    # Assign a global ID to each reticle in the system
    def add_global_reticle_ids(self) -> None:
        global_id = 0
        for wafer in self.wafers:
            for reticle in wafer.reticles:
                reticle.attributes["global_reticle_id"] = global_id
                global_id += 1
        return None

    # Add neighbor information to each reticle in the system
    def add_neighbor_information(self) -> None:
        # Add the "neighbors" attribute to each reticle in the system.
        for wafer in self.wafers:
            for reticle in wafer.reticles:
                reticle.attributes["neighbors"] = []
        # Iterate through wafers
        for (wid, wafer) in enumerate(self.wafers):
            # Iterate through reticles
            for (rid, reticle) in enumerate(wafer.reticles):
                # Check connectivity to the wafer below if it exists
                # The wafer above is not checked since once the wafer above will be processed, it will insert the links 
                if wid > 0:
                    owid = wid - 1
                    other_wafer = self.wafers[owid]
                    # Iterate through reticles in the other wafer
                    for (orid, other_reticle) in enumerate(other_wafer.reticles):
                        # Iterate through vertical connectors in the current reticle
                        for (vcid, vc) in enumerate(reticle.vertical_connectors):
                            # Iterate through vertical connectors in the other reticle
                            for (ovcid, ovc) in enumerate(other_reticle.vertical_connectors):
                                # Check if the vertical connections overlap
                                if vc.overlaps(ovc):
                                    reticle.attributes["neighbors"].append((owid, orid, ovcid, vcid))
                                    other_reticle.attributes["neighbors"].append((wid, rid, vcid, ovcid))


    # Generate a map that maps each combination of (global_reticle_id, vertical_connector_id) to a router_id
    # Note that one reticle can have one or multiple routers and that a router can be connected to one of multiple vertical connectors (all in the same reticle)
    # Also generate a mapping from router_id to the positions of the routers
    def generate_router_id_map_and_locations_and_node_counts(self) -> None:
        # Verify that the global_reticle_id has been assigned to each reticle
        if not all("global_reticle_id" in reticle.attributes for wafer in self.wafers for reticle in wafer.reticles):
            self.add_global_reticle_ids()
        # Create the mapping
        router_id_map = {}
        router_locations = []
        node_counts = []
        next_router_id = 0
        for (wid, wafer) in enumerate(self.wafers):
            for reticle in wafer.reticles:
                # Reticles with the "central_router" topology contain one router
                if reticle.noc_topology == "central_router":
                    # Iterate through vertical connectors
                    for (vcid, vc) in enumerate(reticle.vertical_connectors):
                        router_id_map[(reticle.attributes["global_reticle_id"], vcid)] = next_router_id
                    next_router_id += 1
                    router_locations.append([reticle.x, reticle.y])
                    node_counts.append(self.concentration if reticle.typ == "compute" else 0)
                elif reticle.noc_topology == "fully_connected":
                    for (vcid, vc) in enumerate(reticle.vertical_connectors):
                        router_id_map[(reticle.attributes["global_reticle_id"], vcid)] = next_router_id
                        next_router_id += 1
                        router_locations.append([vc.x, vc.y])
                        if reticle.typ != "compute":
                            node_counts.append(0)
                        else:
                            hlp.register_error("Reticle of type 'compute' with 'fully_connected' NoC topology is not supported.")
                elif reticle.noc_topology == "concentration_2":
                    vc_coords = [(vc.x, vc.y) for vc in reticle.vertical_connectors]
                    router_coords = hlp.pair_closest(vc_coords)
                    tmp_loc_to_router_id = {}
                    for (router_id, ((x1,y1), (x2,y2))) in enumerate(router_coords):
                        tmp_loc_to_router_id[(x1,y1)] = next_router_id
                        tmp_loc_to_router_id[(x2,y2)] = next_router_id
                        router_x = (x1 + x2) / 2
                        router_y = (y1 + y2) / 2
                        next_router_id += 1
                        router_locations.append([router_x, router_y])
                        if reticle.typ != "compute":
                            node_counts.append(0)
                        else:
                            hlp.register_error("Reticle of type 'compute' with 'concentration_2' NoC topology is not supported.")
                    for (vcid, vc) in enumerate(reticle.vertical_connectors):
                        router_id_map[(reticle.attributes["global_reticle_id"], vcid)] = tmp_loc_to_router_id[(vc.x, vc.y)]
                else:
                    hlp.register_error("Unsupported NoC topology '%s' in reticle, only 'central_router' and 'fully_connected' ar  e supported." % reticle.noc_topology)
        # Store the results
        self.router_id_map = router_id_map
        self.router_locations = router_locations
        self.node_counts = node_counts

    # Generate the adjacency list 
    def get_adj_list_and_link_latencies(self, link_latency_fuction : Callable[[float],int]) -> tuple[list[list[int]], list[list[float]], list[list[int]]]:
        # Verify that the global_reticle_id has been assigned to each reticle
        if not all("global_reticle_id" in reticle.attributes for wafer in self.wafers for reticle in wafer.reticles):
            self.add_global_reticle_ids()
        # Verify that the neighbor information has been added
        if not all("neighbors" in reticle.attributes for wafer in self.wafers for reticle in wafer.reticles):
            self.add_neighbor_information()
        # Compute the mapping from (global_reticle_id, vertical_connector_id) to router_id if not already done
        if not hasattr(self, 'router_id_map') or not hasattr(self, 'router_locations'):
            self.generate_router_id_map_and_locations_and_node_counts()
        n_routers = max(self.router_id_map.values()) + 1
        # Compose adjacency list based on neighbor information
        adj_list = [[] for _i in range(n_routers)]
        link_lengths = [[] for _i in range(n_routers)]
        link_latencies = [[] for _i in range(n_routers)]
        for (wid, wafer) in enumerate(self.wafers):
            for (rid, ret) in enumerate(wafer.reticles):
                # Add links between different routers in the same reticle
                iternal_router_ids = list(set(self.router_id_map[(ret.attributes["global_reticle_id"], vcid)] for vcid in range(len(ret.vertical_connectors))))
                for router_id_1 in iternal_router_ids:
                    for router_id_2 in iternal_router_ids:
                        if router_id_1 != router_id_2:
                            if router_id_2 not in adj_list[router_id_1]:
                                adj_list[router_id_1].append(router_id_2)
                                link_length = hlp.compute_manhattan_distance(self.router_locations[router_id_1], self.router_locations[router_id_2])
                                link_lengths[router_id_1].append(link_length)
                                link_latencies[router_id_1].append(link_latency_fuction(link_length))
                # Add links between different reticles based on neighbor information
                for (owid, orid, ovcid, vcid) in ret.attributes['neighbors']:
                    router_id_1 = self.router_id_map[(ret.attributes["global_reticle_id"], vcid)]
                    router_id_2 = self.router_id_map[(self.wafers[owid].reticles[orid].attributes["global_reticle_id"], ovcid)]
                    if router_id_2 not in adj_list[router_id_1]:
                        adj_list[router_id_1].append(router_id_2)
                        link_length = hlp.compute_manhattan_distance(self.router_locations[router_id_1], self.router_locations[router_id_2])
                        link_lengths[router_id_1].append(link_length)
                        link_latencies[router_id_1].append(link_latency_fuction(link_length))
        # Return the adjacency list
        return adj_list, link_lengths, link_latencies

    # Get the router locations list
    def get_router_locations(self) -> list[tuple[float,float]]:
        # Compute router locations if not already done
        if not hasattr(self, 'router_locations'):
            self.router_id_map = self.generate_router_id_map_and_locations_and_node_counts()
        return self.router_locations

    # Get the node count per router
    def get_node_counts(self) -> list[int]:
        # Compute node counts if not already done
        if not hasattr(self, 'node_counts'):
            self.compute_router_id_map_and_locations_and_node_counts()
        return self.node_counts

def construct_system(design : Dict, parameters : Dict) -> System:
    # Extract general parameters
    reticle_size = parameters["reticle_size"]
    sp = parameters["shape_param"]
    node_count = parameters["node_count"]
    # Extract design parameters
    integration_level = design["integration_level"]
    wafer_diameter = design["wafer_diameter"]
    wafer_utilization = design["wafer_utilization"]
    method = design["method"]
    # Prepare the wafers
    wafers = []
    if integration_level == "logic_and_interconnect":
        # Compute and interconnect wafer
        if method == "baseline":
            wafers.append(create_wafer(None, wafer_diameter, "compute", reticle_size, "compute", wafer_utilization, "rectangles", {}))
            wafers.append(create_wafer(wafers[0], wafer_diameter, "interconnect", reticle_size, "interconnect", wafer_utilization, "rectangles-on-corners", {}))
        elif method == "ours_aligned":
            wafers.append(create_wafer(None, wafer_diameter, "compute", reticle_size, "compute", wafer_utilization, "rectangles_2", {}))
            wafers.append(create_wafer(wafers[0], wafer_diameter, "interconnect", reticle_size, "interconnect", wafer_utilization, "rectangles-on-edges", {"method" : "aligned", "indent": 0}))
        elif method == "ours_interleaved":
            wafers.append(create_wafer(None, wafer_diameter, "compute", reticle_size, "compute", wafer_utilization, "rectangles_2", {}))
            wafers.append(create_wafer(wafers[0], wafer_diameter, "interconnect", reticle_size, "interconnect", wafer_utilization, "rectangles-on-edges", {"method" : "interleaved", "indent": 0}))
        elif method == "ours_rotated":
            wafers.append(create_wafer(None, wafer_diameter, "compute", reticle_size, "compute", wafer_utilization, "rotated_lower", {}))
            wafers.append(create_wafer(wafers[0], wafer_diameter, "interconnect", reticle_size, "interconnect", wafer_utilization, "rotated_upper", {}))
        else:
            hlp.register_error("Unknown method %s not supported for integration level %s." % (method, integration_level))
    elif integration_level == "logic_and_logic":
        k = 2  # Number of compute wafers
        for layer in range(k):
            placement = ""
            if method == "baseline":
                placement = "rectangles" if layer % 2 == 0 else "rectangles-on-corners"
            elif method == "ours_aligned":
                placement = "H-interleaved" if layer % 2 == 0 else "plus-interleaved"
            else:
                hlp.register_error("Unknown method %s not supported for integration level %s." % (method, integration_level))
            wafer_below = wafers[-1] if len(wafers) > 0 else None
            wafers.append(create_wafer(wafer_below, wafer_diameter, "compute", reticle_size, "compute", wafer_utilization, placement, {"shape_param" : sp}))
    else:
        hlp.register_error("Unknown integration level %s not supported." % integration_level)
    # Create system
    system = System(wafers, design["integration_level"], design["wafer_diameter"], design["wafer_utilization"], design["method"], node_count)
    # Return system
    return system





