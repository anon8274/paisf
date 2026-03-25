#include "prefix_routing.hpp"
#include <vector>
#include <queue>
#include <tuple>
#include <map>
#include <set>
#include <climits>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <string>
#include <sstream>

// Deadlock-free: YES
// Minimal: NO
// Deterministic: YES
// Description: Assign labels to routers 
// Settings: Shortest paths w.r.t. hops vs. shortest paths w.r.t. latency
std::vector<std::tuple<int,int,int>> prefix_routing::route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vsc, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const {
	int out_port = -1;
	// if we already reached the destination router, route to the destination node
	if (cur_rid == dst_rid) {
		out_port = std::get<0>(router_list[0][dst_rid][dst_nid]);
	// We are not yet at the destination router and need to use prefix routing
	} else {
		// If next hop has not been computed yet, compute it
		if (routing_table.find(cur_rid) == routing_table.end() || routing_table.at(cur_rid).find(dst_rid) == routing_table.at(cur_rid).end()) {
			route_from_source_to_destination(router_list, n_routers, n_nodes, cur_rid, dst_rid);
		}
		out_port = routing_table.at(cur_rid).at(dst_rid);
	}
	return {std::make_tuple(out_port, 0, n_vsc - 1)};
}

// Internal helper function to compute the length of the common prefix of two labels
int prefix_routing::get_prefix_length(const std::string& label_1, const std::string& label_2) const {
    std::stringstream s1(label_1), s2(label_2);
    std::string t1, t2;
    int len = 0;

    while (std::getline(s1, t1, '|')) {
        if (!std::getline(s2, t2, '|')) return -1;
        if (t1 != t2) return -1;
        ++len;
    }
    return len;
}


// In order to make the algorithm work for arbitrary router radices (not only up to 9), we insert the character "|" between any two labels, s.t. one label can cosiest of more than one digit.
void prefix_routing::assign_labels(std::vector<std::map<int, std::map<int, std::tuple<int,int,int>>>>& router_list, int n_routers) const {
    int root_rid = rand() % n_routers;
	// NOTE: Hard-coded distance metric: true = latency, false = hops
    bool use_latency_for_distance = false;

	// Prim's algorithm to build MST
    std::vector<bool> visited(n_routers, false);
    std::vector<int> distance(n_routers, INT_MAX);
    std::vector<int> parent(n_routers, -1);
	std::vector<int> child_count(n_routers, 0);
    std::set<int> todo;

	// The router and link labels are defined as attribute of the routing function class. The line below is just for reference.
    // std::vector<std::string> router_labels(n_routers, "");	// router_labels[rid] = label
	// std::map<std::pair<int,int>, std::string> link_labels;	// link_labels[{cur_rid,nxt_rid}] = label
	// Initialize router_labels
	router_labels = std::vector<std::string>(n_routers, "");

	// Start from root
    distance[root_rid] = 0;
    router_labels[root_rid] = "1";
    todo.insert(root_rid);

	// Variables used during loop
	int cur_rid, nei_rid, nxt_rid;
	int min_dist, new_distance, link_latency;


	// Build MST using Prim's algorithm and assign router labels
    while (!todo.empty()) {
        // pick closest unvisited node
        cur_rid = -1;
        min_dist = INT_MAX;
        for (std::set<int>::iterator it = todo.begin(); it != todo.end(); ++it) {
			nei_rid = *it;
            if (distance[nei_rid] < min_dist) {
                min_dist = distance[nei_rid];
                cur_rid = nei_rid;
            }
        }
		// mark cur as visited
        todo.erase(cur_rid);
        visited[cur_rid] = true;
		// Processing or all nodes except root
		if (parent[cur_rid] != -1) {
			// Increment child count of parent
			child_count[parent[cur_rid]] += 1;
			// Assign label according to the definition of prefix routing (see paper)
			router_labels[cur_rid] = router_labels[parent[cur_rid]] + "|" + std::to_string(child_count[parent[cur_rid]] + 1);
		}
        // relax edges (Prim step)
		for (std::map<int,std::tuple<int,int,int>>::iterator it = router_list[1][cur_rid].begin(); it != router_list[1][cur_rid].end(); ++it) {
            nei_rid = it->first;
            if (visited[nei_rid]){
				continue;
			}
			link_latency = std::get<2>(it->second);
			new_distance = use_latency_for_distance ? link_latency : 1;
			if (new_distance < distance[nei_rid]) {
				// Insert into todo. Since todo is a set, duplicates are ignored.
				todo.insert(nei_rid);
				// Update distance and parent. These are overwritten whenever a better distance is found.
				distance[nei_rid] = new_distance;
				parent[nei_rid] = cur_rid;
			}
        }
    }

	// Sanity-check that all routers have been visited
	for (int rid = 0; rid < n_routers; rid++) {
		assert(visited[rid] && "Not all routers were visited during MST construction!");
	}

	// Iterate trough all links and assign link labels
	for (std::map<int,std::map<int,std::tuple<int,int,int>>>::iterator cur_it = router_list[1].begin(); cur_it != router_list[1].end(); ++cur_it) {
		cur_rid = cur_it->first;
		// Iterate through all neighbors of cur_rid
		for (std::map<int,std::tuple<int,int,int>>::iterator nxt_it = cur_it->second.begin(); nxt_it != cur_it->second.end(); ++nxt_it) {
			nxt_rid = nxt_it->first;
			// If cur_rid is the parent of nxt_rid, then the edge parent->child gets the label of the child
			if (parent[nxt_rid] == cur_rid) {
				link_labels[std::make_pair(cur_rid,nxt_rid)] = router_labels[nxt_rid];
			// If nxt_rid is the parent of cur_rid, then the edge child->parent gets the empty label
			} else if (parent[cur_rid] == nxt_rid) {
				link_labels[std::make_pair(cur_rid,nxt_rid)] = "";
			// No parent-child relationship between cur_rid and nxt_rid: The link gets the label of the destination (nxt_rid)
			} else {
				link_labels[std::make_pair(cur_rid,nxt_rid)] = router_labels[nxt_rid];
			}
		}
	}
}

void prefix_routing::route_from_source_to_destination(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int src_rid, int dst_rid) const {
	int cur_rid = src_rid;
	int max_prefix_len = 0;
	int best_out_port = -1;
	std::vector<int> up_channels;
	// Call the function to assign labels if not done yet
	if (router_labels.empty() || link_labels.empty()) {
		assign_labels(router_list, n_routers);
	}
	// Iterate through outgoing links
	for (std::map<int,std::tuple<int,int,int>>::iterator it = router_list[1][cur_rid].begin(); it != router_list[1][cur_rid].end(); ++it) {
		int nei_rid = it->first;
		std::pair<int,int> link = std::make_pair(cur_rid,nei_rid);
		std::string link_label = link_labels.at(link);
		// Find longest prefix match between link label and destination router label
		int prefix_len = get_prefix_length(link_label, router_labels[dst_rid]);
		if (prefix_len > max_prefix_len) {
			max_prefix_len = prefix_len;
			best_out_port = std::get<0>(it->second);
		}
		// Memorize all up channels (links leading to parent)
		if (link_label == "") {
			up_channels.push_back(std::get<0>(it->second));
		}
	}
	// We should have exactly one up-channel, unless we are at the root
	if (router_labels[cur_rid] != "1") {
		assert(up_channels.size() == 1 && "There should be exactly one up channel!");
	}
	// Use the output port with the longest prefix match, if any
	if (best_out_port != -1) {
		routing_table[cur_rid][dst_rid] = best_out_port;
	// Otherwise, use the up channel (if any)
	} else if (!up_channels.empty()) {
		routing_table[cur_rid][dst_rid] = up_channels[0]; // We use index 0 since there should be exactly one up channel
	} else {
		// This should never happen
		assert(false && "No valid next hop found!");
	}
}



