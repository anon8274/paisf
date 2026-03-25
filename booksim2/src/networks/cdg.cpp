#include "cdg.hpp"
#include <iostream>
#include <cassert>
#include <stack>
#include <set>
#include <map>
#include <climits>
#include <vector>
#include <utility>
#include <functional>
#include <algorithm>

using namespace std;

// Initializes CDG with nodes (one node per virtual channel) but does not add any edges yet.
CDG::CDG(const vector<map<int,map<int,tuple<int,int,int>>>>& router_list, int n_routers, int n_vcs) : _router_list(router_list) {
	// Store router list and VC count
	_n_routers = n_routers;
	_n_vcs = n_vcs;
	// Variables used throughput function
	int cur_rid, nxt_rid, vc;
	// Initialize path id counter
	_path_id_counter = 0;
	// Iterate through routers
	for (cur_rid = 0; cur_rid < n_routers; ++cur_rid) {
		// Iterate through outgoing links of the router
		for (const pair<const int, tuple<int,int,int>>& item : router_list.at(1).at(cur_rid)) {
			nxt_rid = item.first;
			for (vc = 0; vc < n_vcs; ++vc) {
				// Initialize adjacency list with empty vectors for each virtual channel
				_cdg_adj_list[make_tuple(cur_rid, nxt_rid, vc)] = vector<tuple<int,int,int>>();   
			}
		}
	}
}

void CDG::increase_vcs(int number_of_additional_vcs){
	// Variables used throughput function
	int cur_rid, nxt_rid, vc;
	// Iterate through routers
	for (cur_rid = 0; cur_rid < _n_routers; ++cur_rid) {
		// Iterate through outgoing links of the router
		for (const pair<const int, tuple<int,int,int>>& item : _router_list.at(1).at(cur_rid)) {
			nxt_rid = item.first;
			for (vc = _n_vcs; vc < _n_vcs + number_of_additional_vcs; ++vc) {
				// Initialize adjacency list with empty vectors for each virtual channel
				_cdg_adj_list[make_tuple(cur_rid, nxt_rid, vc)] = vector<tuple<int,int,int>>();   
			}
		}
	}
	// Update VC count
	_n_vcs += number_of_additional_vcs;
}

void CDG::insert_path(vector<tuple<int,int,int>>& path) {
	// Variables used throughout function
	tuple<int,int,int> from_node, to_node;
	// Add path to internal list of paths
	int path_id = _path_id_counter++;
	_paths[path_id] = path;
	// Traverse path and add edges to CDG adjacency list if not already present
	for (size_t i = 0; i < path.size() - 1; ++i) {
		from_node = path[i];
		to_node = path[i+1];
		// Add edge from from_node to to_node if not already present
		if (find(_cdg_adj_list[from_node].begin(), _cdg_adj_list[from_node].end(), to_node) == _cdg_adj_list[from_node].end()) {
			_cdg_adj_list[from_node].push_back(to_node);						// Add edge to CDG
			_edge_to_paths[make_pair(from_node, to_node)] = vector<int>();   // Initialize edge_to_paths entry for this new edge
		}
		// Link this path to the edge (from_node, to_node) in edge_to_paths
		_edge_to_paths[make_pair(from_node, to_node)].push_back(path_id);
	}
}

void CDG::remove_path(int path_id) {
	// Check if path ID is valid
	if (_paths.find(path_id) == _paths.end()) {
		cerr << "CDG: Error: Path ID " << path_id << " does not exist." << endl;
		exit(1);
	}
	// Variables used throughout function
	tuple<int,int,int> from_node, to_node;
	vector<tuple<int,int,int>> path = _paths[path_id];
	// Traverse path and remove edges from CDG adjacency list if this is the only path using the edge
	for (size_t i = 0; i < path.size() - 1; ++i) {
		from_node = path[i];
		to_node = path[i+1];
		// Remove this path from edge_to_paths for the edge (from_node, to_node)
		vector<int>& edge_to_paths_entry = _edge_to_paths[make_pair(from_node, to_node)];
		edge_to_paths_entry.erase(remove(edge_to_paths_entry.begin(), edge_to_paths_entry.end(), path_id), edge_to_paths_entry.end());
		// If no more paths use this edge, remove it from the CDG adjacency list
		if (edge_to_paths_entry.empty()) {
			_cdg_adj_list[from_node].erase(remove(_cdg_adj_list[from_node].begin(), _cdg_adj_list[from_node].end(), to_node), _cdg_adj_list[from_node].end());
			_edge_to_paths.erase(make_pair(from_node, to_node));  // Clean up edge_to_paths entry for unused edge
		}
	}
	// Remove path from internal list of paths
	_paths.erase(path_id);
}

void CDG::remove_path(vector<tuple<int,int,int>>& path) {
	// Find path ID for the given path
	int path_id = -1;
	for (const pair<const int, vector<tuple<int,int,int>>>& item : _paths) {
		if (item.second == path) {
			path_id = item.first;
			break;
		}
	}
	if (path_id == -1) {
		cerr << "CDG: Error: Path not found." << endl;
		exit(1);
	}
	remove_path(path_id);  // Remove path using the other remove_path function
}

vector<vector<tuple<int,int,int>>> CDG::identify_one_cycle_and_remove_min_number_of_paths_to_break_it() {
    vector<vector<tuple<int,int,int>>> removed_paths;
    set<tuple<int,int,int>> visited;
    set<tuple<int,int,int>> in_stack; 
    stack<pair<tuple<int,int,int>, bool>> dfs_stack; // (node, processed-flag)
    map<tuple<int,int,int>, tuple<int,int,int>> parent;
	// Variables used throughout function
    tuple<int,int,int> from_node, to_node;
    vector<int> path_ids_using_edge;
	// Iterate through all nodes in CDG to make sure all disconnected components are covered
    for (const pair<const tuple<int,int,int>, vector<tuple<int,int,int>>>& item : _cdg_adj_list) {
        tuple<int,int,int> start_node = item.first;
		// Only process this node if it hasn't been visited yet (i.e., it's part of a new disconnected component)
        if (visited.find(start_node) == visited.end()) {
			// Perform DFS starting from this node
            dfs_stack.push(make_pair(start_node, false));
            parent[start_node] = make_tuple(-1, -1, -1);
            while (!dfs_stack.empty()) {
                pair<tuple<int,int,int>, bool> top = dfs_stack.top();
                dfs_stack.pop();
                tuple<int,int,int> current_node = top.first;
                bool processed = top.second;
				// If this is the exit phase for current_node, we remove it from in_stack and mark it as visited
                if (processed) {
                    in_stack.erase(current_node);
                    visited.insert(current_node);
                    continue;
                } else {
					// If this is the enter phase, but the node has already been visited, we skip it.
					if (visited.find(current_node) != visited.end()) {
						continue;
					} else {
						// If this is the enter phase and the node has not been visited, we schedule the exit phase and add neighbors to the stack
						dfs_stack.push(make_pair(current_node, true));
						in_stack.insert(current_node);
						// Explore neighbors of current node
						for (const tuple<int,int,int>& neighbor : _cdg_adj_list.at(current_node)) {
							// The node is in the stack, and we encounter it again → cycle detected
							if (in_stack.find(neighbor) != in_stack.end()) {
								// Explicitly backtrack to find the cycle path using the parent map
								vector<tuple<int,int,int>> cycle;
								tuple<int,int,int> temp = current_node;
								while (temp != make_tuple(-1, -1, -1)) {
									cycle.push_back(temp);
									temp = parent[temp];
								}
								reverse(cycle.begin(), cycle.end());
								cycle.push_back(neighbor);
								// Identify the edge in the cycle that is used by the fewest number of paths
								int min_paths = INT_MAX;
								pair<tuple<int,int,int>, tuple<int,int,int>> edge_to_remove;
								for (size_t i = 0; i + 1 < cycle.size(); ++i) {
									from_node = cycle[i];
									to_node   = cycle[i+1];
									int num_paths = _edge_to_paths[make_pair(from_node, to_node)].size();
									if (num_paths < min_paths) {
										min_paths = num_paths;
										edge_to_remove = make_pair(from_node, to_node);
									}
								}
								// Identify all paths that use the edge to remove and remove them from the CDG
								path_ids_using_edge = _edge_to_paths[edge_to_remove];
								for (int path_id : path_ids_using_edge) {
									removed_paths.push_back(_paths[path_id]);
									remove_path(path_id);
								}
								// Return the list of removed paths after breaking the cycle
								return removed_paths;
							}
							// If the neighbor hasn't been visited, we add it to the stack for processing
							if (visited.find(neighbor) == visited.end()) {
								dfs_stack.push(make_pair(neighbor, false));
								parent[neighbor] = current_node;
							}
							// If neither of the two above conditions are true, it means the neighbor has already been visited and is not in the current path, so we can safely ignore it without adding to the stack.
						}
					}
				}
			}
        }
    }
    return removed_paths;
}


bool CDG::is_cyclic() const {
    set<tuple<int,int,int>> visited;
    set<tuple<int,int,int>> in_stack;
    stack<pair<tuple<int,int,int>, bool>> dfs_stack; // Holds pairs of (node, is_exit_phase)
	// Iterate through all nodes in CDG to make sure all disconnected components are covered
    for (const pair<const tuple<int,int,int>, vector<tuple<int,int,int>>>& item : _cdg_adj_list) {
        tuple<int,int,int> start_node = item.first;
		// Only process this node if it hasn't been visited yet (i.e., it's part of a new disconnected component)
        if (visited.find(start_node) == visited.end()) {
			// Perform DFS starting from this node
            dfs_stack.push(make_pair(start_node, false));
            while (!dfs_stack.empty()) {
                pair<tuple<int,int,int>, bool> top = dfs_stack.top();
                dfs_stack.pop();
                tuple<int,int,int> current_node = top.first;
                bool processed = top.second;
				// If this is the exit phase for current_node, we remove it from in_stack and mark it as visited
                if (processed) {
                    in_stack.erase(current_node);
                    visited.insert(current_node);
                    continue;
                } else {
					// If this is the enter phase, but the node has already been visited, we skip it.
					if (visited.find(current_node) != visited.end()) {
						continue;
					} else {
						// If this is the enter phase and the node has not been visited, we schedule the exit phase and add neighbors to the stack
						dfs_stack.push(make_pair(current_node, true));  
						in_stack.insert(current_node);
						for (const tuple<int,int,int>& neighbor : _cdg_adj_list.at(current_node)) {
							// The node is in the stack, and we encounter it again → cycle detected
							if (in_stack.find(neighbor) != in_stack.end()) {
								return true;
							}
							// If the neighbor hasn't been visited, we add it to the stack for processing
							if (visited.find(neighbor) == visited.end()) {
								dfs_stack.push(make_pair(neighbor, false));
							}
							// If neither of the two above conditions are true, it means the neighbor has already been visited and is not in the current path, so we can safely ignore it without adding to the stack.
						}
					}
				}
            }
        }
    }
    return false;
}
