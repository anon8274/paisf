#include "shortest_path_vc_increment_routing.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <tuple>
#include <vector>
#include <map>
#include <set>
#include <climits>

// Deadlock-free: YES
// Minimal: YES
// Deterministic: NO
// Description: Shortest path routing; VC is incremented at each hop; Network needs at least as many VCs as the longest path in the network
// Settings: Shortest paths w.r.t. hops vs. shortest paths w.r.t. latency
std::vector<std::tuple<int,int,int>> shortest_path_vc_increment_routing::route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vsc, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const {
	// If the routing table has been precomputed for the current router, then compute it
	if (routing_table.find(cur_rid) == routing_table.end() || routing_table.at(cur_rid).find(dst_nid) == routing_table.at(cur_rid).end()) {
		route_from_source(router_list, n_routers, n_nodes, cur_rid);
	}
	// Read the output port from the routing table
	// Here, we only return a single output port (deterministic routing)
	std::vector<int> out_ports = routing_table[cur_rid][dst_nid];
	// Select the next VC, if this is the first hop, start at VC 0 otherwise increment the VC
	int out_vc;
	if (cur_rid == src_rid) {
		out_vc = 0;
	}
	else {
		out_vc = cur_vc + 1;
	}
	// Ensure that the next VC is valid (can be invalid if the network diameter is larger than the number of VCs)
	if (out_vc >= n_vsc) {
		std::cout << "ERROR: For shortest_path_vc_increment_routing, the number of VCs must be larger or equal to the network diameter! Number of VCs: " << n_vsc << ", required minimum: " << out_vc + 1 << std::endl;
		exit(1);
	}
	std::vector<std::tuple<int,int,int>> valid_next_hops;
	for (int out_port : out_ports) {
		valid_next_hops.push_back(std::make_tuple(out_port, out_vc, out_vc));
	}
	// Return all output ports with the next VC
	return valid_next_hops;
}

// Computes shortest paths from the source router to all nodes in the network and sets the routing table accordingly
// Only sets the routing table entries of the current source router (cur_rid)
void shortest_path_vc_increment_routing::route_from_source(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int src_rid) const {
	// NOTE: Hard-coded distance metric: true = latency, false = hops
	bool use_latency_for_distance = false;
	// Data structures for Dijkstra's algorithm
	std::vector<int> dist(n_routers, INT_MAX);
	std::vector<std::vector<int>> prevs(n_routers, std::vector<int>());
	std::set<int> todo;
	// Auxiliary variables (allocate only once)
	int link_latency;
	int nei_rid;
	std::set<int> nei_rids;
	std::set<int> nei_rids_tmp;
	int new_distance;
	int nid;
	int port;
	std::vector<int> ports;
	// Initialize the source router
	dist[src_rid] = 0;
	todo.insert(src_rid);
	// Dijkstra's algorithm main loop
	while (!todo.empty()) {
		// Find the router with the smallest distance (and lowest ID in case of ties)
		int cur_rid = -1;
		for (int rid : todo) {
			if (cur_rid == -1 || dist[rid] < dist[cur_rid] || (dist[rid] == dist[cur_rid] && rid < cur_rid)) {
				cur_rid = rid;
			}
		}
		todo.erase(cur_rid);
		// Explore neighbors of the current router
		for (std::map<int,std::tuple<int,int,int>>::iterator it = router_list[1][cur_rid].begin(); it != router_list[1][cur_rid].end(); ++it) {
			nei_rid = it->first;	
			link_latency = std::get<2>(it->second);
			new_distance = dist[cur_rid] + (use_latency_for_distance ? link_latency : 1);
			if (new_distance < dist[nei_rid]) {
				dist[nei_rid] = new_distance;
				prevs[nei_rid].clear();
				prevs[nei_rid].push_back(cur_rid);
				todo.insert(nei_rid);
			} else if (new_distance == dist[nei_rid]) {
				prevs[nei_rid].push_back(cur_rid);
			}
		}
	}
	// Use back-tracking to set the routing table entries for all nodes in the network
	for (int dst_rid = 0; dst_rid < n_routers; ++dst_rid) {
		// Source router: 
		if (dst_rid == src_rid) {
			// Add the correct output port for all local nodes (only one possible output port)
			for (std::map<int,std::tuple<int,int,int>>::iterator it = router_list[0][src_rid].begin(); it != router_list[0][src_rid].end(); ++it) {
				nid = it->first;		
				port = std::get<0>(it->second);
				routing_table[src_rid][nid].push_back(port);
			}
		// Other routers:
		} else {
			// Back-track to find the next hop and set the output port accordingly
			nei_rids.clear();
			nei_rids_tmp.clear();
			nei_rids_tmp.insert(dst_rid);
			while(!nei_rids_tmp.empty()) {
				nei_rids = nei_rids_tmp;
				nei_rids_tmp.clear();
				for (int nei_rid : nei_rids) {
					if (std::find(prevs[nei_rid].begin(), prevs[nei_rid].end(), src_rid) == prevs[nei_rid].end() && !prevs[nei_rid].empty()) {
						for (int prev_rid : prevs[nei_rid]) {
							nei_rids_tmp.insert(prev_rid);
						}
					}
				}
			}
			ports.clear();
			for (int nei_rid : nei_rids) {
				ports.push_back(std::get<0>(router_list[1][src_rid][nei_rid]));
			}
			// Add that direction for all nodes that are attached to that destination router
			for (std::map<int,std::tuple<int,int,int>>::iterator it = router_list[0][dst_rid].begin(); it != router_list[0][dst_rid].end(); ++it) {
				nid = it->first;		
				for (int port : ports) {
					routing_table[src_rid][nid].push_back(port);
				}
			}
		}	
	}
}
