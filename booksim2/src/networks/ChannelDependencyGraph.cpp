#include "ChannelDependencyGraph.hpp"
#include <iostream>
#include <cassert>
#include <stack>
#include <set>
#include <map>
#include <vector>
#include <utility>
#include <functional>
#include <algorithm>

// Builds the channel dependency graph from the routing function and router list
ChannelDependencyGraph::ChannelDependencyGraph(const std::map<int,int>& node_list, const std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>>& router_list, std::map<int, std::map<int, int>> router_and_outport_to_next_router, int n_routers, int n_nodes, int n_vcs, const routing_function& rf) {
	std::cout << "Building Channel Dependency Graph... " << std::endl;
	// Helper variables only allocated once
	std::vector<std::tuple<int,int,int>> valid_next_hops;
	std::tuple<int,int,int> next_hop;
	std::vector<std::tuple<int,int,int,int>> hops;		// prev_rid, cur_rid, in_port, virtual_channel
	std::vector<std::tuple<int,int,int,int>> new_hops;	// prev_rid, cur_rid, in_port, virtual_channel
	int src_rid, dst_rid, prev_rid, cur_rid, next_rid;
	int src_nid, dst_nid;
	int out_port, in_port, next_in_port;
	int vc, min_vc, max_vc, next_vc;
	int livelock_threshold = 2 * n_routers;
	// Initialize adjacency list of the channel dependency graph
	for (std::map<int,std::map<int,std::tuple<int,int,int>>>::const_iterator sit = router_list[1].begin(); sit != router_list[1].end(); ++sit) {
		src_rid = sit->first;
		for (std::map<int,std::tuple<int,int,int>>::const_iterator dit = sit->second.begin(); dit != sit->second.end(); ++dit) {
			dst_rid = dit->first;
			out_port = std::get<0>(dit->second);
			for (vc = 0; vc < n_vcs; ++vc) {
				cdg_adj_list[std::make_tuple(src_rid, dst_rid, vc)] = std::vector<std::tuple<int,int,int>>{};
			}
		}
	}
	// All routing algorithms that need the flit-ID to track the available hops left (only done for algorithms that allow for non-shortest paths)
	// we use negative flit-IDs to ignore the hop-limit and always return all available next hops / paths. We use this for the CDG checker.
	// Some algorithms store the source of a given flit-id, so we need to use different flit-IDs for different source-destination pairs.
	int flit_id = -1;
	// Construct the Channel Dependency Graph
	for (src_nid = 0; src_nid < n_nodes; ++src_nid) {
		src_rid = node_list.at(src_nid);
		for (dst_nid = 0; dst_nid < n_nodes; ++dst_nid) {
			if (src_nid == dst_nid) continue;
			dst_rid = node_list.at(dst_nid);
			in_port = std::get<1>(router_list[0].at(src_rid).at(src_nid));
			// Due to the possibility of multiple paths, we need to track a series of concurrent hops
			// Each hop is given as a tuple of (prev_rid, cur_rid, in_port, virtual_channel) where at injection, the VC is always assumed to be 0.
			hops.clear();
			hops.push_back(std::make_tuple(-1, src_rid, in_port, 0));
			// Continue until we reached the router connected to the destination node
			int hop_count = 0;
			while (!hops.empty() && hop_count < livelock_threshold) {
				new_hops.clear();
				// Iterate through all current hops
				for (std::vector<std::tuple<int,int,int,int>>::const_iterator hit = hops.begin(); hit != hops.end(); ++hit) {
					prev_rid = std::get<0>(*hit);
					cur_rid = std::get<1>(*hit);
					in_port = std::get<2>(*hit);
					vc = std::get<3>(*hit);
					// We only need to do something if the destination has not been reached
					if (cur_rid != node_list.at(dst_nid)) {
						valid_next_hops = rf.route(router_list, n_routers, n_nodes, n_vcs, src_rid, cur_rid, vc, dst_nid, dst_rid, in_port, flit_id);
						// For each valid next hop, create a new hop and add it to new_hops
						for (std::vector<std::tuple<int,int,int>>::const_iterator vhit = valid_next_hops.begin(); vhit != valid_next_hops.end(); ++vhit) {
							out_port = std::get<0>(*vhit);
							min_vc = std::get<1>(*vhit);
							max_vc = std::get<2>(*vhit);
							next_rid = router_and_outport_to_next_router.at(cur_rid).at(out_port);
							next_in_port = std::get<1>(router_list[1].at(cur_rid).at(next_rid));
							// Expand for all possible virtual channels
							for (next_vc = min_vc; next_vc <= max_vc; ++next_vc) {
								// Add the new hop to the list, but only if it does not exist yet. This allows the re-convergence of paths
								if (std::find(new_hops.begin(), new_hops.end(), std::make_tuple(cur_rid, next_rid, next_in_port, next_vc)) == new_hops.end()){
									new_hops.push_back(std::make_tuple(cur_rid, next_rid, next_in_port, next_vc));
								}
								// If there was a previous router, add an edge to the CDG
								if (prev_rid != -1) {
									std::tuple<int,int,int> cdg_src_vertex = std::make_tuple(prev_rid, cur_rid, vc);
									std::tuple<int,int,int> cdg_dst_vertex = std::make_tuple(cur_rid, next_rid, next_vc);
									// Only add an edge if it is not already present
									// Since the cdg_adj_list is initialized with all possible vertices, cdg_src_vertex must exist, otherwise there is a bug
									if (std::find(cdg_adj_list[cdg_src_vertex].begin(), cdg_adj_list[cdg_src_vertex].end(), cdg_dst_vertex) == cdg_adj_list[cdg_src_vertex].end()){
										cdg_adj_list[cdg_src_vertex].push_back(cdg_dst_vertex);
									}	
								}
							}
						}
					}
				}
				hops = new_hops;
				hop_count++;
			}
			assert(hop_count < livelock_threshold && "Potential livelock detected during CDG construction. ABORTING.");
			flit_id--;	
		}
	}
	std::cout << " Done!" << std::endl;
}

bool ChannelDependencyGraph::is_acyclic() const {
	// Data structures for DFS
    std::set<std::tuple<int,int,int>> visited;
    std::set<std::tuple<int,int,int>> stack;

	std::cout << "Checking Channel Dependency Graph for cycles... ";
	// Use adjacency list for easier traversal
    std::function<bool(const std::tuple<int,int,int>&)> dfs = [&](const std::tuple<int,int,int>& u) -> bool {
            if (stack.count(u) > 0) return true;		// Cycle detected
            if (visited.count(u) > 0) return false;		// Already visited node, no cycle from this node
			// Mark the current node as visited and add to recursion stack
            visited.insert(u);
            stack.insert(u);
			// Recur for all neighbors
            std::map<std::tuple<int,int,int>, std::vector<std::tuple<int,int,int>>>::const_iterator it = cdg_adj_list.find(u);
            if (it != cdg_adj_list.end()) {
                const std::vector<std::tuple<int,int,int>>& neighbors = it->second;
                for (std::vector<std::tuple<int,int,int>>::const_iterator vit = neighbors.begin(); vit != neighbors.end(); ++vit) {
                    if (dfs(*vit)) return true;			// Cycle detected in the recursion
                }
            }
            stack.erase(u);
            return false;								// No cycle detected from this node
        };
	// Call the recursive helper function to detect cycle in different DFS trees
    for (std::map<std::tuple<int,int,int>, std::vector<std::tuple<int,int,int>>>::const_iterator it = cdg_adj_list.begin(); it != cdg_adj_list.end(); ++it) {
        const std::tuple<int,int,int>& node = it->first;
        if (visited.count(node) == 0 && dfs(node)) {
			std::cout << " Done!" << std::endl;
            return false;								// Cycle found, graph is not acyclic		
        }
    }
	return true;										// No cycles found, graph is acyclic
}

