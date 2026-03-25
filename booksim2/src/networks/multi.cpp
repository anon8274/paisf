#include "multi.hpp"
#include <map>
#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <algorithm>
#include <functional>
#include <string>
#include <tuple>
#include <set>
#include <cmath>

using namespace std;


// Enumerates valid paths from src_rid to dst_rid that follow the restrictions given by the routing function.
// Each path is represented as a vector of hops, where each hop is a tuple of (current router id, next router id, vc).
// This function does NOT enumerate all possible paths, instead, it uses the heuristic Algorithm 1 that is described in the "Multi" paper (#376 in personal reading list)
vector<vector<tuple<int,int,int>>> enumerate_paths(vector<map<int, map<int, tuple<int,int,int>>>> router_list, map<int, map<int, int>> router_and_outport_to_next_router, routing_function& rf, int src_rid, int dst_rid, int n_routers, int n_nodes, int n_vcs) {
	// Make sure to never use the same flit_id for multiple src-dst pairs (can break some routing functions).
	static int flit_id = 0;
	// Identify the first node that is attached to the destination router
	int dst_nid = -1;
	if (!router_list[0][dst_rid].empty()) {
		dst_nid = router_list[0][dst_rid].begin()->first;
	} else {
		cerr << "Error: Destination router " << dst_rid << " has no attached nodes." << endl;
		exit(1);
	}
	// Identify the first node that is attached to the source router and use it to get the input port at the source router
	int src_in_port = -1;
	if (!router_list[0][src_rid].empty()) {
		src_in_port = get<1>(router_list[0][src_rid].begin()->second);
	} else {
		cerr << "Error: Source router " << src_rid << " has no attached nodes." << endl;
		exit(1);
	}
	// We use VC0 for injection at the source router.
	int src_in_vc = 0;
	// List of paths found so far. Format: paths[i] = [(cur_rid, nxt_rid, vc), ...] for the i-th path from src_rid to dst_rid
	vector<vector<tuple<int,int,int>>> paths;
	// Map that states whether a given edge is valid or not (invalid means it was removed according to line (2) in Algorithm 1 in the "Multi" paper)
	// Edgs are given by (rid1, rid2, vc), where rid1 is the current router, rid2 is the next router, and vc is the virtual channel used on the edge from rid1 to rid2
	map<tuple<int,int,int>, bool> edge_is_valid;
	int other_rid;
	for (int rid = 0; rid < n_routers; rid++) {
		for (map<int, tuple<int,int,int>>::iterator it = router_list[1][rid].begin(); it != router_list[1][rid].end(); it++) {
			other_rid = it->first;
			for (int vc = 0; vc < n_vcs; vc++) {
				edge_is_valid[make_tuple(rid, other_rid, vc)] = true;
			}
		}
	}
	// Variables used in the while loop below
	bool path_found = true;
	int prev_rid, cur_rid, nxt_rid, rid1, rid2, vc, middle_index, random_index, in_port, in_vc, out_port, min_vc, max_vc;
	vector<tuple<int,int,int>> cur_path;
	// We use a stack to perform a depth-first search to find a path from src_rid to dst_rid that follows the restrictions given by the routing function and the valid edges given by the edge_is_valid map.	
	vector<tuple<int,int,int>> dfs_stack; // Format: (cur_rid, in_port, in_vc)
	// While loop that aborts once no more paths can be found
	// One iterations corresponds to finding one path from src_rid to dst_rid and removing the middle edge of that path (line (2) in Algorithm 1 in the "Multi" paper)
	// We use one flit-ID per path. Flit-IDs are used by routing function that support non-minimal paths to bound the path length.
	while (path_found) {
		cur_path.clear();
		dfs_stack.clear();
		cur_rid = src_rid;	
		in_port = src_in_port;
		in_vc = src_in_vc;
		while (cur_rid != dst_rid) {
			// Possible next hops according to the routing function. Format: [(output_port, min_vc, max_vc), ...]
			vector<tuple<int,int,int>> valid_next_hops_rf = rf.route(router_list, n_routers, n_nodes, n_vcs, src_rid, cur_rid, in_vc, dst_nid, dst_rid, in_port, flit_id);
			// Filter the list of possible next hops by checking if the corresponding edges are valid according to the edge_is_valid map.
			// Expand for the (output_port, min_vc, max_vc) format to the (nxt_rid, vc) format
			vector<pair<int,int>> valid_next_hops;
			for (tuple<int,int,int> hop : valid_next_hops_rf) {
				out_port = get<0>(hop);
				min_vc = get<1>(hop);
				max_vc = get<2>(hop);
				// Identify the next router id corresponding to the output port
				if (router_and_outport_to_next_router[cur_rid].find(out_port) != router_and_outport_to_next_router[cur_rid].end()) {
					other_rid = router_and_outport_to_next_router[cur_rid][out_port];
					for (int vc = min_vc; vc <= max_vc; vc++) {
						if (edge_is_valid[make_tuple(cur_rid, other_rid, vc)]) {
							valid_next_hops.push_back(make_pair(other_rid, vc));
						}
					}
				} else {
					cerr << "Error: No next router found for router " << cur_rid << " and output port " << out_port << "." << endl;
					exit(1);
				}
			}
			// Check if there is at least one valid next hop left after filtering. If not, break the while loop and conclude that no more path can be found.
			if (valid_next_hops.empty()) {
				// If there are still elements in the DFS stack, we backtrack to the previous point in the DFS and try to find another path from there.
				// If the stack is empty, it means that we have backtracked all the way to the source router and there is no more path to be found.
				if (!dfs_stack.empty()) {
					// Remove the last hop from the current path since we are backtracking from that hop.
					if (!cur_path.empty()) {
						cur_path.pop_back();
					} else {
						cerr << "Error: Current path is empty while trying to backtrack. This should not happen." << endl;
						exit(1);
					}
					// Increment the hops budget of the current flit in the routing function since we are backtracking from a dead end.
					// This only has an effect for routing functions that support non-minimal paths and use the flit-ID to bound the path length.
					// For all other routing functions, this does not have any effect.
					rf.increment_hop_budget(flit_id);
					// Fetch the last stack frame of the DFS stack and remove it from the stack.
					tuple<int,int,int> last_stack_frame = dfs_stack.back();
					dfs_stack.pop_back();
					// If we reached a dead end, we invalidate the edge that led us to the dead end.
					// In traditional DFS, we would just set the vertex to "visited", but with deadlock-avoidance based on turn restrictions, we cannot do that
					// because a vertex might be a dead end when entering over one edge, but not when entering over another edge
					// Also, if the edge is a dead end in this iteration of the outer loop, it will not be a dead end in all future iterations of the outer loop,
					// Hence, we invalidate the edge for all future iterations of the outer loop.
					prev_rid = get<0>(last_stack_frame);
					edge_is_valid[make_tuple(prev_rid, cur_rid, in_vc)] = false;
					// Use the fetched stack frame to update the current router, input port, and input vc for the next iteration of the inner while loop (i.e. backtrack one level in the DFS)
					cur_rid = get<0>(last_stack_frame);
					in_port = get<1>(last_stack_frame);
					in_vc = get<2>(last_stack_frame);
					continue;
				} else {
					break;
				}
			}
			// If this part of the code is reached, it means that there is at least one valid next hop. We pick the first one and add it to the current path.
			// We select of potentially multiple valid next hops uniformly at random.
			random_index = rand() % valid_next_hops.size();
			nxt_rid = valid_next_hops[random_index].first;
			vc = valid_next_hops[random_index].second;
			// Add the selected next hop to the current path
			cur_path.push_back(make_tuple(cur_rid, nxt_rid, vc));
			// Add the current point in DFS to the stack and move to the next hop
			dfs_stack.push_back(make_tuple(cur_rid, in_port, in_vc));
			// Update the current router, input port, and input vc for the next iteration of the inner while loop (i.e. go one level deeper in the DFS)
			in_port = get<1>(router_list[1][cur_rid][nxt_rid]);
			in_vc = vc;
			cur_rid = nxt_rid;
		}
		// If another path was found
		if (cur_rid == dst_rid){
			path_found = true;
			// Add the path found above to the list of paths found so far
			paths.push_back(cur_path);
			// Remove the middle edge of the path found above (line (2) in Algorithm 1 in the "Multi" paper)
			middle_index = cur_path.size() / 2;
			rid1 = get<0>(cur_path[middle_index]);
			rid2 = get<1>(cur_path[middle_index]);
			vc = get<2>(cur_path[middle_index]);
			edge_is_valid[make_tuple(rid1, rid2, vc)] = false;
		// If no more path can be found, break the while loop
		} else {
			path_found = false;
		}
		flit_id++;
	}
	// Sanity check: Make sure that at least one path was found. If not, print an error message and exit.
	if (paths.empty()) {
		cerr << "Error: No path found from source router " << src_rid << " to destination router " << dst_rid << "." << endl;
		exit(1);
	}
	// Return the paths found
	return paths;
}

// This function takes a list of paths from the source to the destination and heuristically tries to select a maximum subset of those paths that are edge-disjoint.
// This function implements Algorithm 2 that is described in the "Multi" paper (#376 in personal reading list).
vector<vector<tuple<int,int,int>>> select_edge_disjoint_paths(vector<vector<tuple<int,int,int>>> paths, int n_routers){
	// Step 1: 	Build a compatibility graph that contains one vertex for each path and an edge between two vertices if the corresponding paths are edge-disjoint.
	// 			We represent this graph as an adjacency list where we use the index within the path vector as the vertex id
	vector<vector<int>> compatibility_graph(paths.size());
	for (size_t i = 0; i < paths.size(); i++) {
		for (size_t j = i + 1; j < paths.size(); j++) {
			bool are_edge_disjoint = true;
			for (tuple<int,int,int> hop_i : paths[i]) {
				for (tuple<int,int,int> hop_j : paths[j]) {
					if (get<0>(hop_i) == get<0>(hop_j) && get<1>(hop_i) == get<1>(hop_j) && get<2>(hop_i) == get<2>(hop_j)) {
						are_edge_disjoint = false;
						break;
					}
				}
				if (!are_edge_disjoint) {
					break;
				}
			}
			if (are_edge_disjoint) {
				compatibility_graph[i].push_back(j);
				compatibility_graph[j].push_back(i);
			}
		}
	}
	// Step 2: Find the vertex with the highest degree (path with the most edge-disjoint paths) and add it to the selected paths.
	vector<vector<tuple<int,int,int>>> selected_paths;
	int max_degree = -1;
	int degree;
	vector<int> path_indices_with_max_degree;
	for (size_t i = 0; i < compatibility_graph.size(); i++) {
		degree = compatibility_graph[i].size();
		if (degree > max_degree) {
			max_degree = degree;
			path_indices_with_max_degree.clear();
			path_indices_with_max_degree.push_back(i);
		} else if (degree == max_degree) {
			path_indices_with_max_degree.push_back(i);
		}
	}
	// Randomly select one of the paths with the maximum degree and add it to the selected paths
	int random_index = rand() % path_indices_with_max_degree.size();
	int path_index = path_indices_with_max_degree[random_index];
	selected_paths.push_back(paths[path_index]);
	// Step 3 and 4: From the remaining vertices (paths) greedily add as many as possible as long as they are compatible (edge-disjoint) with the already selected paths.
	vector<int> candidate_path_indices = compatibility_graph[path_index];
	while (!candidate_path_indices.empty()) {
		random_index = rand() % candidate_path_indices.size();
		path_index = candidate_path_indices[random_index];
		selected_paths.push_back(paths[path_index]);
		// Remove all vertices (paths) that are not compatible (edge-disjoint) with the path that was just added to the selected paths
		vector<int> new_candidate_path_indices;
		for (int candidate_path_index : candidate_path_indices) {
			if (candidate_path_index != path_index) {
				bool is_compatible = true;
				for (tuple<int,int,int> hop_i : paths[path_index]) {
					for (tuple<int,int,int> hop_j : paths[candidate_path_index]) {
						if (get<0>(hop_i) == get<0>(hop_j) && get<1>(hop_i) == get<1>(hop_j) && get<2>(hop_i) == get<2>(hop_j)) {
							is_compatible = false;
							break;
						}
					}
					if (!is_compatible) {
						break;
					}
				}
				if (is_compatible) {
					new_candidate_path_indices.push_back(candidate_path_index);
				}
			}
		}
		candidate_path_indices = new_candidate_path_indices;
	}
	// Sanity check: Make sure that we found at least one path. If not, print an error message and exit.
	if (selected_paths.empty()) {
		cerr << "Error: No edge-disjoint path found." << endl;
		exit(1);
	}
	// Return the selected paths
	return selected_paths;
}

void multi_compute_distinct_paths_for_one_src_dst_pair(map<int, map<tuple<int,int,int,int>, vector<tuple<int,int,int>>>>& multi_routing_table, map<int,int> node_list, vector<map<int, map<int, tuple<int,int,int>>>> router_list, map<int, map<int, int>> router_and_outport_to_next_router, int n_vcs, int src_rid, int dst_rid, routing_function& rf){
	// Sanity check
	if (router_list[0].size() != router_list[1].size()) {
		cerr << "Error: Inconsistent router list sizes in multi_compute_distinct_paths_for_one_src_dst_pair." << endl;
		exit(1);
	}
	// Extract parameters
	int n_routers = router_list[0].size();
	int n_nodes = node_list.size();
	// Heuristically compute paths from src_rid to dst_rid that follow the restrictions given by the routing function.
	// The paths are stored in the "paths" variable. Format: paths[i] = [(cur_rid, nxt_rid, vc), ...] for the i-th path from src_rid to dst_rid
	// We use Algorithm 1 that is described in the "Multi" paper to heuristically compute the paths.
	// Note that this algorithm does not guarantee to find all possible paths.
	vector<vector<tuple<int,int,int>>> paths = enumerate_paths(router_list, router_and_outport_to_next_router, rf, src_rid, dst_rid, n_routers, n_nodes, n_vcs);
	// Heuristically select a maximum subset of the paths computed above that are edge-disjoint (i.e., they do not share any edge).
	// We use Algorithm 2 that is described in the "Multi" paper to heuristically select the edge-disjoint paths.
	// Note that this algorithm does not guarantee to find the maximum subset of edge-disjoint paths.
	paths = select_edge_disjoint_paths(paths, n_routers);
	// Variables used in the loop below	
	int last_vc, in_port, rid1, rid2, vc, out_port;
	// Configure the computed paths in the multi routing table
	// paths[i] = [(cur_rid, nxt_rid, vc), ...] for the i-th path from src_rid to dst_rid
	for (vector<tuple<int,int,int>> path : paths) {
		last_vc = 0; // 0 is used for injection, i.e., on the first hop, the incoming vc is 0
		in_port = 0; // use 0 as input port for a node (first hop); Make sure to use 0 instead of the real input port when accessing the routing table
		for (tuple<int,int,int> hop : path) {
			rid1 = get<0>(hop);
			rid2 = get<1>(hop);
			vc = get<2>(hop);
			out_port = get<0>(router_list[1][rid1][rid2]);
			multi_routing_table[rid1][make_tuple(src_rid, dst_rid, in_port, last_vc)].push_back(make_tuple(out_port, vc, vc));
			last_vc = vc;
			in_port = get<1>(router_list[1][rid1][rid2]);
		}
	}
}
