# Import python modules
import sys
import random as rnd
from types import SimpleNamespace

# Import custom modules
import helpers as hlp

# Iterative version of DFS to check if the network is fully connected
def is_connected(adj_list : list[list[int]], faulty_vertices : list[int], faulty_edges : list[tuple[int,int]]) -> bool:
    num_nodes = len(adj_list)
    visited = [False] * num_nodes
    start_vertex = min(set(range(num_nodes)) - set(faulty_vertices)) # Start at first non-faulty vertex
    stack = [start_vertex]
    visited[start_vertex] = True
    while stack:
        node = stack.pop()
        # Sanity check: Faulty nodes should not be added to the stack
        for neighbor in adj_list[node]:
            # Skip faulty vertices and edges
            if neighbor in faulty_vertices or (node, neighbor) in faulty_edges or (neighbor, node) in faulty_edges:
                continue
            # The link to the neighbor is valid and neighbor is also valid and has not been visited yet
            if not visited[neighbor]:
                visited[neighbor] = True
                stack.append(neighbor)
    is_connected = [visited[i] for i in range(num_nodes) if i not in faulty_vertices]
    return all(is_connected)

# Randomly removes links from the network based on the fault rate
# It is of paramount importance to use the same random seed for all combinations of routing and selection functions in order to get identical faulty networks
# If this is not done, some combinations may end up with worse fault patterns that are harder to route around, leading to unfair comparisons
# This functions ensures that the network stays fully connected after fault insertion
# If the network becomes disconnected, it retries up to max_attempts times before giving up and returning None
def insert_link_faults(network : dict, fault_rate : float, random_seed : int, max_attempts : int) -> dict:
    adj_list = network['adj_list']            # Changed when link faults are inserted
    lengths = network['lengths']                    # Changed when link faults are inserted
    latencies = network['latencies']                # Changed when link faults are inserted
    node_counts = network['node_counts']            # Unchanged
    node_latencies = network['node_latencies']      # Unchanged
    router_locations = network['router_locations']  # Unchanged
    # Set random seed
    rnd.seed(random_seed)
    # List of all edges in the network
    all_edges = [(i, j) for i in range(len(adj_list)) for j in adj_list[i] if i < j] # We only store each bidirectional edge once
    num_faulty_edges = int(len(all_edges) * fault_rate)
    faulty_edges = []
    for i in range(num_faulty_edges):
        faulty_edge = None
        for attempt in range(max_attempts):
            fault_candidate = rnd.choice(all_edges)
            if fault_candidate not in faulty_edges and is_connected(adj_list, [], faulty_edges + [fault_candidate]):
                faulty_edge = fault_candidate
                break
        if faulty_edge is not None:
            faulty_edges.append(faulty_edge)
        else:
            hlp.print_error(f"Failed to insert link faults while maintaining connectivity after {max_attempts} attempts.")
            return None
    # Create new network with faulty edges. Note that the number of vertices and hence their ids remain unchanged
    for rid1 in range(len(adj_list)):
        for link_idx in range(len(adj_list[rid1]) - 1, -1, -1):
            rid2 = adj_list[rid1][link_idx]
            if (rid1, rid2) in faulty_edges or (rid2, rid1) in faulty_edges:
                adj_list[rid1].pop(link_idx)
                lengths[rid1].pop(link_idx)
                latencies[rid1].pop(link_idx)
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network


def insert_router_faults(network : dict, fault_rate : float, random_seed : int, max_attempts : int) -> dict:
    adj_list = network['adj_list']            # Changed when router faults are inserted
    lengths = network['lengths']                    # Changed when router faults are inserted
    latencies = network['latencies']                # Changed when router faults are inserted
    node_counts = network['node_counts']            # Changed when router faults are inserted
    node_latencies = network['node_latencies']      # Changed when router faults are inserted
    router_locations = network['router_locations']  # Changed when router faults are inserted
    # Set random seed
    rnd.seed(random_seed)
    num_nodes = len(adj_list)
    num_faulty_nodes = int(num_nodes * fault_rate)
    faulty_vertices = []
    for i in range(num_faulty_nodes):
        faulty_vertex = None
        for attempt in range(max_attempts):
            fault_candidate = rnd.randint(0, num_nodes - 1)
            if fault_candidate not in faulty_vertices and is_connected(adj_list, faulty_vertices + [fault_candidate], []):
                faulty_vertex = fault_candidate
                break
        if faulty_vertex is not None:
            faulty_vertices.append(faulty_vertex)
        else:
            hlp.print_error(f"Failed to insert router faults while maintaining connectivity after {max_attempts} attempts.")
            return None
    # Construct a map that maps old router ids to new router ids after faulty routers have been removed
    # This map only contains non-faulty routers
    router_id_map = {old_id : new_id for new_id, old_id in enumerate(sorted(set(range(num_nodes)) - set(faulty_vertices)))}
    # Pass one: Remove all links from any router to a faulty router and remap the remaining router ids
    for rid1 in range(len(adj_list)):
        for link_idx in range(len(adj_list[rid1]) - 1, -1, -1):
            rid2 = adj_list[rid1][link_idx]
            if rid2 in faulty_vertices:
                adj_list[rid1].pop(link_idx)
                lengths[rid1].pop(link_idx)
                latencies[rid1].pop(link_idx)
            else:
                adj_list[rid1][link_idx] = router_id_map[rid2]
    # Pass two: Remove all faulty routers from the network
    for faulty_router in sorted(faulty_vertices, reverse=True):
        adj_list.pop(faulty_router)
        lengths.pop(faulty_router)
        latencies.pop(faulty_router)
        node_counts.pop(faulty_router)
        node_latencies.pop(faulty_router)
        router_locations.pop(faulty_router)
    # Create new network with faulty vertices. Note that the number of vertices and hence their ids remain unchanged
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network
