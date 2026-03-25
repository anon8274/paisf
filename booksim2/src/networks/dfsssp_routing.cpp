#include "dfsssp_routing.hpp"
#include "cdg.hpp"
#include <queue>
#include <vector>
#include <iostream>
#include <algorithm>
#include <tuple>
#include <vector>
#include <map>
#include <set>
#include <climits>

using namespace std;

void dfsssp_routing::initialize(vector<map<int,map<int,tuple<int,int,int>>>> router_list, int n_routers, int n_vcs) const {
	// Pre-computations: Identify all routers that are connected to nodes
	_routers_with_nodes.clear();
	for (int rid = 0; rid < n_routers; rid++) {
		if (router_list[0][rid].size() > 0) {
			_routers_with_nodes.push_back(rid);
		}
	}
	// Step 1: Compute shortest paths according to Algorithm 1 in the DFSSSP paper. All paths will be assigned to layer 0, potentially causing deadlocks.
	sssp_routing(router_list, n_routers);
	// Step 2: Assign paths to layers according to Algorithm 2 in the DFSSSP paper. This step ensures that the resulting routing algorithm is deadlock-free.
	search_and_remove_deadlocks(router_list, n_routers, n_vcs);
	// Step 3: Balance path across available virtual channels
	// TODO: Implement path balancing
	// Mark as initialized
	// TODO: Currently only need to get number of VCs needed, hence exit after init.
	exit(0);
	_is_initialized = true;

}


// Compute shortest paths according to Algorithm 1 in the DFSSSP paper. All paths will be assigned to layer 0, potentially causing deadlocks.
void dfsssp_routing::sssp_routing(vector<map<int,map<int,tuple<int,int,int>>>> router_list, int n_routers) const {
	// Initialize the edge weights as described in DFSSSP paper (page 2, bottom-right).
	int init_weight = n_routers * n_routers;
	map<pair<int,int>, int> edge_weights;
	for (int rid = 0; rid < n_routers; rid++) {
		for (const pair<const int, tuple<int,int,int>>& item : router_list[1][rid]) {
			edge_weights[make_pair(rid, item.first)] = init_weight;
		}
	}
	// Variables reused throughout the algorithm
	int cur_rid, cur_dist, nxt_rid, out_port;
	// Iterate through all destination routers that are connected to nodes and compute shortest paths to them
	for (int dst : _routers_with_nodes) {
		priority_queue<pair<int,int>, vector<pair<int,int>>, greater<>> queue;
		vector<int> dist(n_routers, INT_MAX);
		vector<int> next(n_routers, -1);
		dist[dst] = 0;
		queue.push({0, dst});
		// Dijkstra's algorithm to find shortest paths to a given dst w.r.t. edge weights.
		while (!queue.empty()) {
			// Get the router with the smallest distance to dst from the priority queue
			pair<int,int> current = queue.top();
			queue.pop();
			cur_dist = current.first;
			cur_rid = current.second;
			// Skip in case this element is outdated (i.e. we have already found a shorter path to cur_rid)
			if (cur_dist > dist[cur_rid]) {
				continue;
			}
			// Iterate through all neighbors of cur_rid
			for (const pair<const int, tuple<int,int,int>>& item : router_list[1][cur_rid]) {
				nxt_rid = item.first;
				pair<int,int> edge = make_pair(cur_rid, nxt_rid);
				if (dist[cur_rid] + edge_weights[edge] < dist[nxt_rid]) {
					dist[nxt_rid] = dist[cur_rid] + edge_weights[edge];
					next[nxt_rid] = cur_rid;
					queue.push({dist[nxt_rid], nxt_rid}); // Add pair with updated distance to priority queue
				}
			}
		}
		// Set routing table entries for all routers that can reach dst and update edge weights accordingly
		for (int src : _routers_with_nodes) {
			// If we are already at the destination router, the route() function will directly route to the destination node without using the routing table.
			if (src == dst){
				continue;
			}
			// If src cannot reach dst, we throw an error as the network is not fully connected.
			if (dist[src] == INT_MAX) {
				cerr << "Error: Router " << src << " cannot reach destination router " << dst << ". The network is not fully connected." << endl;
				exit(1);
			}
			// Configure routing table entries and set edge weights for this src-dst pair
			cur_rid = src;
			while (cur_rid != dst) {
				nxt_rid = next[cur_rid];
				out_port = get<0>(router_list[1][cur_rid][nxt_rid]);
				_routing_table[cur_rid][src][dst] = make_tuple(out_port, 0, nxt_rid); // Assign path to layer 0
				edge_weights[make_pair(cur_rid, nxt_rid)] += 1;	
				cur_rid = nxt_rid;
			}
		}
	}
}

// Assign paths to layers according to Algorithm 2 in the DFSSSP paper. This step ensures that the resulting routing algorithm is deadlock-free.
void dfsssp_routing::search_and_remove_deadlocks(vector<map<int,map<int,tuple<int,int,int>>>> router_list, int n_routers, int n_vcs) const {
	// Variables reused throughout the algorithm
	tuple<int,int,int> hop;
	tuple<int,int,int> port_vc_nxt_rid;
	int cur_rid, src_rid, dst_rid, out_port;
	// Initialize the CDG with all paths
	CDG cdg = CDG(router_list, n_routers, n_vcs);
	for (int src : _routers_with_nodes) {
		for (int dst : _routers_with_nodes) {
			if (src != dst) {
				vector<tuple<int,int,int>> path;
				cur_rid = src;
				while (cur_rid != dst) {
					port_vc_nxt_rid = _routing_table[cur_rid][src][dst];
					path.push_back(make_tuple(cur_rid, get<2>(port_vc_nxt_rid), get<1>(port_vc_nxt_rid)));
					cur_rid = get<2>(port_vc_nxt_rid);
				}
				cdg.insert_path(path);
			}
		}
	}
	int max_vc = 0;
	// Move paths for the current to the next virtual layer until there are no more cycles in the CDG that use the current virtual layer.
	while (cdg.is_cyclic()){
		vector<vector<tuple<int,int,int>>> paths_to_move =  cdg.identify_one_cycle_and_remove_min_number_of_paths_to_break_it();
		// In DFSSSP, a path should always be on just one layer, hence, all hops of all removed paths should be on the same layer. Verify this.
		int cur_vc = -1;
		for (vector<tuple<int,int,int>>& path : paths_to_move) {
			for (tuple<int,int,int>& hop : path) {
				if (cur_vc == -1) {
					cur_vc = get<2>(hop);
				} else if (get<2>(hop) != cur_vc) {
					cerr << "DFSSSP: Error: Identified paths for cycle breaking are not all on the same layer. This should not happen in DFSSSP." << endl;
					exit(1);
				}
			}
		}
		int new_vc = cur_vc + 1;
		if (new_vc >= n_vcs && new_vc > max_vc) {
			// Add an extra VC to the CDG even though this one does not exist in the actual network.
			// This is only done to identify the number of required VCs for deadlock-free routing.
			// In those cases, BookSim will be aborted after initialization with an error message.
			cdg.increase_vcs(1);	
		}
		max_vc = max(max_vc, new_vc);
		// Move all identified paths to the next virtual layer in the routing table and update the CDG accordingly.
		for (vector<tuple<int,int,int>>& path : paths_to_move) {
			src_rid = get<0>(path[0]);
			dst_rid = get<1>(path[path.size() - 1]);
			for (tuple<int,int,int>& hop : path) {
				// Edit the paths directly. This is used to updat the CDG
				get<2>(hop) = new_vc;
				// Edit the routing table entries for this hop. This is used for the route() function.
				cur_rid = get<0>(hop);
				out_port = get<0>(_routing_table[cur_rid][src_rid][dst_rid]);
				_routing_table[cur_rid][src_rid][dst_rid] = make_tuple(out_port, new_vc, get<1>(hop));
			}
			// Insert the updated path with the new virtual layer into the CDG
			cdg.insert_path(path);
		}
	}
	// TODO: Debug output; Remove later
	std::cout << "This topology requires " << (max_vc + 1) << " layers, " << n_vcs << " were assigned" << std::endl;
	// TODO: Final check - should not be needed
	cout << "CDG is " << (cdg.is_cyclic() ? "cyclic" : "acyclic") << " after deadlock removal." << endl;
	if (max_vc >= n_vcs) {
		cerr << "Error: DFSSSP requires " << (max_vc + 1) << " VCs for deadlock-free routing, but only " << n_vcs << " are available." << endl;
		exit(1);
	}
}

// Deadlock-free: YES
// Minimal: YES
// Deterministic: YES
vector<tuple<int,int,int>> dfsssp_routing::route(vector<map<int,map<int,tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const {
	// Initialize, i.e. compute routing tables
	if (!_is_initialized) {
		initialize(router_list, n_routers, n_vcs);
	}
	int output_port, output_vc;
	// If we are already at the destination router, we can directly route to the destination node
	if (cur_rid == dst_rid) {
		// Identify output port to dst_nid
		output_port = get<0>(router_list[0][cur_rid][dst_nid]);
		output_vc = 0; // Use VC 0 for Ejection
	} else {
		// Read next hop from routing table
		tuple<int,int,int> next_hop = _routing_table[cur_rid][src_rid][dst_rid];
		output_port = get<0>(next_hop);
		output_vc = get<1>(next_hop);
	}
	return vector<tuple<int,int,int>>{make_tuple(output_port, output_vc, output_vc)};
}


