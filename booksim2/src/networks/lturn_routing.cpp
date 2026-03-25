#include "lturn_routing.hpp"
#include "../config_utils.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <map>
#include <ostream>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace std;

// Constructor: Reads configuration
lturn_routing::lturn_routing(const Configuration &config) : _config(config) {
	string mode_str = _config.GetStr("threshold_mode");
	threshold_mode = parse_threshold_mode(mode_str);
	if (threshold_mode == ThresholdMode::LIMITED) {
		MAX_HOPS = _config.GetInt("threshold_hops");
		if (MAX_HOPS <= 0) {
			cerr << "Error: max_hops must be positive when using 'limited' mode\n";
			exit(-1);
		}
	} else {
		MAX_HOPS = -1;
	}
}

// Helper function to parse threshold mode from string
lturn_routing::ThresholdMode lturn_routing::parse_threshold_mode(const string &mode_str) const {
	if (mode_str == "shortest" || mode_str == "0")
		return ThresholdMode::SHORTEST;
	if (mode_str == "bounded" || mode_str == "1")
		return ThresholdMode::BOUNDED;
	if (mode_str == "limited" || mode_str == "2")
		return ThresholdMode::LIMITED;
	if (mode_str == "all" || mode_str == "3")
		return ThresholdMode::ALL;

	cerr << "Warning: Unknown threshold_mode '" << mode_str << "', defaulting to 'shortest'\n";
	return ThresholdMode::SHORTEST;
}

// Classifies the channel type between two routers
lturn_routing::ChannelType lturn_routing::classify_channel(int from, int to) const {
	// Same level
	if (level[from] == level[to]) {
		if (width[to] > width[from]) {
			return ChannelType::RU;
		} else {
			return ChannelType::LD;
		}
	}
	// Moving to different level
	if (level[to] < level[from]) {
		// Moving UP
		if (width[to] < width[from]) {
			return ChannelType::LU;
		} else {
			return ChannelType::RU;
		}
	} else {
		// Moving DOWN
		if (width[to] < width[from]) {
			return ChannelType::LD;
		} else {
			return ChannelType::RD;
		}
	}
}

// ================================================
// LOGIC FOR COMPUTING ILLEGAL TURNS
// ================================================

// computes channel type for all neighbors of a node
vector<pair<int, lturn_routing::ChannelType>> lturn_routing::get_outgoing_channels(int node) const {
	vector<pair<int, ChannelType>> channels;
	for (int neighbor : adj[node]) {
		ChannelType ct = classify_channel(node, neighbor);
		channels.push_back({neighbor, ct});
	}
	return channels;
}

bool lturn_routing::is_turn_already_prohibited(int from, int to, ChannelType in_ct, ChannelType out_ct) const {
	if (in_ct == ChannelType::LD) {
		vector<int> ports = get_ports_to_neighbor(from, to);
		for (int port : ports) {
			if (out_ct == ChannelType::RU &&
					port < (int)prohibited_LD_to_RU[from].size() &&
					prohibited_LD_to_RU[from][port]) {
				return true;
			}
			if (out_ct == ChannelType::RD &&
					port < (int)prohibited_LD_to_RD[from].size() &&
					prohibited_LD_to_RD[from][port]) {
				return true;
			}
		}
	}
	return false;
}

bool lturn_routing::is_returning_via_ld(int current, int next, int start_node) const {
	if (next == start_node) {
		ChannelType ct = classify_channel(current, next);
		return (ct == ChannelType::LD);
	}
	return false;
}

bool lturn_routing::find_cycles_from_node(int start_node, bool is_case_a, vector<pair<int, int>> &prohibited_turns) const {
	auto channels = get_outgoing_channels(start_node);
	vector<int> start_neighbors;

	for (const auto &[neighbor, ct] : channels) {
		if ((is_case_a && ct == ChannelType::RU) ||
				(!is_case_a && ct == ChannelType::RD)) {
			start_neighbors.push_back(neighbor);
		}
	}

	bool found_cycle = false;

	for (int first_neighbor : start_neighbors) {
		// BFS queue: (current_node, incoming_ct, depth, parent_node)
		queue<tuple<int, ChannelType, int, int>> q;
		q.push({first_neighbor, classify_channel(start_node, first_neighbor), 1,
						start_node});

		// Track visited (node, incoming_ct) pairs
		set<pair<int, ChannelType>> visited;
		visited.insert(
				{first_neighbor, classify_channel(start_node, first_neighbor)});

		// For backtracking to find the cycle
		map<pair<int, ChannelType>, pair<int, ChannelType>> parent;
		parent[{first_neighbor, classify_channel(start_node, first_neighbor)}] = {start_node, ChannelType::LU}; // Dummy value for start

		while (!q.empty()) {
			auto [current, in_ct, depth, parent_node] = q.front();
			q.pop();

			// Depth limit
			// if (depth > 10)
			//	 continue;

			auto current_channels = get_outgoing_channels(current);

			for (const auto &[next, out_ct] : current_channels) {
				// Skip LU channels
				if (out_ct == ChannelType::LU)
					continue;

				// Check turn legality
				vector<int> ports = get_ports_to_neighbor(current, next);
				if (ports.empty())
					continue;

				if (!is_turn_legal(in_ct, out_ct, current, ports[0])) {
					continue;
				}

				// Avoid LD→RU turns for case (b)
				bool avoid_ld_to_ru = !is_case_a;
				if (avoid_ld_to_ru && in_ct == ChannelType::LD &&
						out_ct == ChannelType::RU) {
					continue;
				}

				// Check if we return to start via LD
				if (next == start_node && out_ct == ChannelType::LD) {
					// Found a cycle!
					prohibited_turns.push_back({current, start_node});
					found_cycle = true;
					return true; // Return immediately
				}

				pair<int, ChannelType> state = {next, out_ct};
				if (visited.find(state) == visited.end()) {
					visited.insert(state);
					parent[state] = {current, in_ct};
					q.push({next, out_ct, depth + 1, current});
				}
			}
		}
	}

	return found_cycle;
}

vector<int> lturn_routing::select_nodes_by_criteria() const {
	// selects node based on amount of RU and RD neighbors to apply
	// two slightly different algorithms for cycle detection
	vector<int> selected_nodes;
	int n_routers = adj.size();

	for (int node = 0; node < n_routers; ++node) {
		int ru_count = 0;
		int rd_count = 0;

		auto channels = get_outgoing_channels(node);
		for (const auto &[neighbor, ct] : channels) {
			if (ct == ChannelType::RU)
				ru_count++;
			else if (ct == ChannelType::RD)
				rd_count++;
		}

		// two or more right-up channels
		if (ru_count >= 2) {
			selected_nodes.push_back(node);
		}
		// one or more right-up and one or more right-down
		else if (ru_count >= 1 && rd_count >= 1) {
			selected_nodes.push_back(node);
		}
	}

	return selected_nodes;
}

void lturn_routing::detect_prohibited_turns(int n_routers) const {
	// Initialize prohibited turn arrays
	int max_port = 0;
	for (auto &m : edge_ports) {
		for (auto &kv : m) {
			for (int p : kv.second) {
				max_port = max(max_port, p);
			}
		}
	}

	prohibited_LD_to_RU.assign(n_routers, vector<bool>(max_port + 1, false));
	prohibited_LD_to_RD.assign(n_routers, vector<bool>(max_port + 1, false));

	// Select nodes
	vector<int> selected_nodes = select_nodes_by_criteria();

	// Process in DFS order
	vector<bool> visited(adj.size(), false);
	vector<int> dfs_order;

	function<void(int)> dfs = [&](int u) {
		visited[u] = true;
		dfs_order.push_back(u);

		vector<pair<int, int>> neighbors;
		for (int v : adj[u]) {
			neighbors.push_back({width[v], v});
		}
		sort(neighbors.begin(), neighbors.end());

		for (auto [w, v] : neighbors) {
			if (!visited[v]) {
				dfs(v);
			}
		}
	};

	dfs(root);

	vector<int> ordered_selected;
	for (int node : dfs_order) {
		if (find(selected_nodes.begin(), selected_nodes.end(), node) !=
				selected_nodes.end()) {
			ordered_selected.push_back(node);
		}
	}

	// Detect cycles and prohibit turns
	vector<pair<int, int>> all_prohibited_turns;

	for (int node : ordered_selected) {
		int ru_count = 0, rd_count = 0;
		auto channels = get_outgoing_channels(node);
		for (const auto &[neighbor, ct] : channels) {
			if (ct == ChannelType::RU)
				ru_count++;
			else if (ct == ChannelType::RD)
				rd_count++;
		}

		bool is_case_a = (ru_count >= 2);

		if (is_case_a) {
		} else if (ru_count >= 1 && rd_count >= 1) {
		} else {
			continue;
		}

		vector<pair<int, int>> cycles_found;
		vector<vector<pair<int, int>>> cycle_paths_for_node;

		if (find_cycles_from_node(node, is_case_a, cycles_found)) {
			for (size_t i = 0; i < cycles_found.size(); i++) {
				auto [ld_node, start] = cycles_found[i];
				all_prohibited_turns.push_back({ld_node, start});
			}
		}
	}

	// Mark prohibited turns
	for (auto [ld_node, start_node] : all_prohibited_turns) {
		for (int neighbor : adj[start_node]) {
			ChannelType out_ct = classify_channel(start_node, neighbor);
			if (out_ct == ChannelType::RU || out_ct == ChannelType::RD) {
				vector<int> ports = get_ports_to_neighbor(start_node, neighbor);
				for (int port : ports) {
					if (port < (int)prohibited_LD_to_RU[start_node].size()) {
						if (out_ct == ChannelType::RU) {
							prohibited_LD_to_RU[start_node][port] = true;
						} else if (out_ct == ChannelType::RD) {
							prohibited_LD_to_RD[start_node][port] = true;
						}
					}
				}
			}
		}
	}
}

// ================================================
// ROUTING LOGIC
// ================================================

// NOTE: Added by PI: Computes the minimum hops to the destination for a given current router and for a given previous router.
void lturn_routing::compute_min_hops_to_dst(const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>> router_list, int n_routers) const {
	// Iterate over all source	
	for (int src = 0; src < n_routers; src++) {
		// Identify all neighbors of cur
		vector<int> neighbors;
		// Iterate through everything in router_list[1][cur] without using auto
		for (map<int, tuple<int, int, int>>::const_iterator it = router_list.at(1).at(src).begin(); it != router_list.at(1).at(src).end(); ++it) {
			neighbors.push_back(it->first);
		}
		neighbors.push_back(-1); // Add -1 to represent the case where we are at the source and have not come from any previous router
		// Iterate through potential previous routers (neighbors of src)
		for (int global_prev : neighbors) {
			// Run Dijkstra from src to all destinations (assuming that we came from global_prev)
			vector<int> dist(n_routers, INT_MAX);
			// We track if edges are visited to allow for visiting the same vertex multiple times (from different directions) 
			// while still avoiding checking the same path multiple times.
			vector<pair<int,int>> visited; 
			// Ou priority queue stores tiples of the form (dist, prev, cur)
			priority_queue<tuple<int,int, int>, vector<tuple<int,int, int>>, greater<tuple<int,int, int>>> pq;
			dist[src] = 0;
			pq.push({0, global_prev, src});
			while (!pq.empty()) {
				auto [cur_dist, prev, cur] = pq.top();
				pq.pop();
				// If we already explore coming from prev to cur, skip
				// (the same (prev,cur) pair can be in the queue with multiple dist values, the smallest one will be processed first)
				if (find(visited.begin(), visited.end(), make_pair(prev, cur)) != visited.end()) {
					continue;
				}
				// Mark edge (prev, cur) as visited
				visited.push_back(make_pair(prev, cur));
				// Identify all neighbors of cur, exclude prev as we do not allow 180 degree turns
				vector<pair<int,int>> cur_neighbors_with_ports;
				for (map<int, tuple<int, int, int>>::const_iterator it = router_list.at(1).at(cur).begin(); it != router_list.at(1).at(cur).end(); ++it) {
					if (it->first != prev) {
						cur_neighbors_with_ports.push_back(make_pair(it->first, get<0>(it->second)));
					}
				}
				// Iterate through neighbors
				for (pair<int,int> nei : cur_neighbors_with_ports) {
					// Avoid 180 degree turns by skipping the neighbor if it is the same as prev
					if (nei.first == prev) {
						continue;
					}
					// Do not us the same edge twice
					if (find(visited.begin(), visited.end(), make_pair(cur, nei.first)) != visited.end()) {
						continue;
					}
					bool is_legal = true;
					// Check if going from prev over cur to nei is a legal turn
					if (prev != -1) {
						ChannelType in_ct = classify_channel(prev, cur);
						ChannelType out_ct = classify_channel(cur, nei.first);
						is_legal = is_turn_legal(in_ct, out_ct, cur, nei.second);
					}
					if (is_legal) {
						// Add this vertex to the queue (also if we did not find a shorter path, we might find a shorter path to a 
						// downstream vertex that is not reachable over the shortest path to nei due to turn restrictions)
						pq.push({cur_dist + 1, cur, nei.first});
						// Update distance to nei if we found a shorter path
						if (cur_dist + 1 < dist[nei.first]) {
							dist[nei.first] = cur_dist + 1;
						}
					}
				}
			}
			// Once Dijkstra is done, we know the shortest distance from src to all destinations when coming from global_prev
			for (int dst = 0; dst < n_routers; dst++) {
				min_hops_to_dst[global_prev][src][dst] = dist[dst];
			}
		}
	}
}

// Alternative Version by PI
vector<int> lturn_routing::compute_next_hops(vector<map<int, map<int, tuple<int, int, int>>>> router_list, int src_rid, int cur_rid, int dst_rid, int prev_rid, int flit_id, ChannelType in_ct) const {
	vector<int> valid_next_hops;

	// Find remaining hops for this flit
	auto it = fID_HOPS.find(flit_id);
	int remaining_hops = it->second;

	// Identify all neighbors of cur, exclude prev as we do not allow 180 degree turns
	vector<pair<int,int>> neighbors_with_ports;
	for (map<int, tuple<int, int, int>>::const_iterator it = router_list.at(1).at(cur_rid).begin(); it != router_list.at(1).at(cur_rid).end(); ++it) {
		// Avoid 180 degree turns by skipping the neighbor if it is the same as prev_rid
		if (it->first != prev_rid) {
			// Only consider neighbors from which the destination is reachable (given that we arrived from cur_rid)
			if (min_hops_to_dst[cur_rid][it->first][dst_rid] < INT_MAX) {
				// Check if going from prev_rid over cur_rid to it->first is a legal turn
				bool is_legal = true;
				if (prev_rid != -1) {
					ChannelType in_ct = classify_channel(prev_rid, cur_rid);
					ChannelType out_ct = classify_channel(cur_rid, it->first);
					is_legal = is_turn_legal(in_ct, out_ct, cur_rid, get<0>(it->second));
				}
				if (is_legal) {
					neighbors_with_ports.push_back(make_pair(it->first, get<0>(it->second)));
				}
			}
		}
	}

	// For shortest paths store shortest available path length
	int shortest_path = INT_MAX;

	// Iterate over valid next hops, check for hop budget according to the threshold mode	
	for (pair<int,int> entry : neighbors_with_ports){
		int nei_rid = entry.first;
		int out_port = entry.second;
		int hops_to_dst_via_port = min_hops_to_dst[cur_rid][nei_rid][dst_rid] + 1; // +1 because we need to add the hop from cur_rid to nei_rid

		if (threshold_mode == ThresholdMode::SHORTEST) {
			// A new shortest path is found, remove all previously stored hops as they were on longer paths
			if (hops_to_dst_via_port < shortest_path) {
				shortest_path = hops_to_dst_via_port;
				valid_next_hops.clear();
			}
			// Add the next hop if it is on a shortest path (supports multiple shortest paths)
			if (hops_to_dst_via_port <= shortest_path) {
				valid_next_hops.push_back(out_port);
			}
		} else {
			if (hops_to_dst_via_port <= remaining_hops) {
				valid_next_hops.push_back(out_port);
			}
		}
	}
	return valid_next_hops;
}

bool lturn_routing::is_turn_legal(ChannelType in_ct, ChannelType out_ct, int router, int port) const {
	// Cannot use LU after non-LU
	if (in_ct != ChannelType::LU && out_ct == ChannelType::LU) {
		return false;
	}

	// Check for prohibited LD→right turns
	if (in_ct == ChannelType::LD) {
		if (out_ct == ChannelType::RU) {
			if (port < (int)prohibited_LD_to_RU[router].size() &&
					prohibited_LD_to_RU[router][port]) {
				return false;
			}
		}
		if (out_ct == ChannelType::RD) {
			if (port < (int)prohibited_LD_to_RD[router].size() &&
					prohibited_LD_to_RD[router][port]) {
				return false;
			}
		}
	}

	return true;
}

vector<int> lturn_routing::get_ports_to_neighbor(int cur, int neighbor) const {
	if (edge_ports[cur].count(neighbor)) {
		return edge_ports[cur].at(neighbor);
	}
	return {};
}

bool lturn_routing::is_tree_edge(int from, int to) const {
	return tree_parent[to] == from || tree_parent[from] == to;
}

int lturn_routing::compute_lr_tree_distance(int src, int dst) const {
    int n_routers = adj.size();
    vector<int> tree_dist(n_routers, INT_MAX);

    queue<int> q;
    tree_dist[src] = 0;
    q.push(src);

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        for (int v : adj[u]) {
            if (is_tree_edge(u, v)) {
                if (v == dst)
                    return tree_dist[u] + 1;
                if (tree_dist[v] == INT_MAX) {
                    tree_dist[v] = tree_dist[u] + 1;
                    q.push(v);
                }
            }
        }
    }
    return -1;
}

void lturn_routing::compute_prev_router_map(
		const vector<map<int, map<int, tuple<int, int, int>>>> &router_list,
		int n_routers) const {

	prev_router_map.clear();

	for (int rid = 0; rid < n_routers; ++rid) {
		if (router_list[1].count(rid)) {
			for (const auto &e : router_list[1].at(rid)) {
				int next = e.first;
				int in_port = get<1>(e.second);
				prev_router_map[next][in_port] = rid;
			}
		}
	}
}

int lturn_routing::get_prev_router(
		int cur, int in_port,
		const map<int, map<int, tuple<int, int, int>>> &connections) const {
	if (in_port < 0)
		return -1;

	if (prev_router_map[cur].count(in_port)) {
		return prev_router_map[cur][in_port];
	}

	return -1;
}
void lturn_routing::initialize(const vector<map<int, map<int, tuple<int, int, int>>>> &router_list, int n_routers) const {

	width.assign(n_routers, -1);
	level.assign(n_routers, -1);

	adj.assign(n_routers, {});
	edge_ports.assign(n_routers, {});
	port_to_neighbor.assign(n_routers, {});

	node_to_router.clear();

	// Node to router map
	for (int r = 0; r < n_routers; ++r) {
		if (router_list[0].count(r)) {
			for (auto &n : router_list[0].at(r)) {
				node_to_router[n.first] = r;
			}
		}
	}

	// Adjacency and ports
	for (int u = 0; u < n_routers; ++u) {
		if (router_list[1].count(u)) {
			for (auto &e : router_list[1].at(u)) {
				int v = e.first;
				int out_port = get<0>(e.second);
				adj[u].push_back(v);
				edge_ports[u][v].push_back(out_port);
				port_to_neighbor[u][out_port] = v;
			}
		}
	}

	// BFS to assign levels and build BFS tree
	vector<bool> visited(n_routers, false);
	queue<int> q;
	vector<vector<int>> bfs_tree(n_routers);
	tree_parent.assign(n_routers, -1);

	// Random root selection
	int random_number = rand();
	root = random_number % n_routers;
	// For debugging
	// root = 0;

	q.push(root);
	level[root] = 0;
	visited[root] = true;

	while (!q.empty()) {
		int cur = q.front();
		q.pop();

		for (int nei : adj[cur]) {
			if (!visited[nei]) {
				visited[nei] = true;
				bfs_tree[cur].push_back(nei);
				level[nei] = level[cur] + 1;
				tree_parent[nei] = cur;
				q.push(nei);
			}
		}
	}

	// DFS to assign widths in BFS tree order
	int counter = 0;
	function<void(int)> dfs = [&](int node) {
		width[node] = counter++;

		sort(bfs_tree[node].begin(), bfs_tree[node].end());

		for (int child : bfs_tree[node]) {
			dfs(child);
		}
	};

	dfs(root);

	compute_prev_router_map(router_list, n_routers);

	// Use the paper's algorithm to detect prohibited turns
	detect_prohibited_turns(n_routers);

	// NOTE: Added by PI
	compute_min_hops_to_dst(router_list, n_routers);

	// Print levels, widths and edges
	// print_mst_topology();
	initialized = true;
}

vector<tuple<int, int, int>> lturn_routing::route(
		vector<map<int, map<int, tuple<int, int, int>>>> router_list, int n_routers,
		int n_nodes, int n_vcs, int src_rid, int cur, int cur_vc, int dst_node, int dst_rid, int in_port,
		int flit_id) const {

	// Make sure initialization is done only once
	if (!initialized) {
		initialize(router_list, n_routers);
	}

	// Get destination router
	int dst_router = node_to_router.at(dst_node);

	// If already at destination router, route to the node
	if (cur == dst_router) {
		for (const auto &node_entry : router_list[0].at(cur)) {
			int nid = node_entry.first;
			if (nid == dst_node) {
				int port = std::get<0>(node_entry.second);
				fID_HOPS.erase(flit_id);
				return {make_tuple(port, 0, n_vcs - 1)};
			}
		}
	}

	// Determine incoming channel type
	ChannelType in_ct = ChannelType::LU; // Default for injection

	// Get previous router based in in_port
	int prev_router = get_prev_router(cur, in_port, router_list[1]);

	// If we are at the course, set the hop budget
	if (prev_router == -1){
		// Allow arbitrary many hops for flits with negative id
		if (flit_id < 0) {
			fID_HOPS.insert({flit_id, n_routers + 1});
		} else {
			if (threshold_mode == ThresholdMode::BOUNDED) {
				vector<int> tree_dist;
				int bound = compute_lr_tree_distance(cur, dst_router);
				fID_HOPS.insert({flit_id, bound});
			} else if (threshold_mode == ThresholdMode::SHORTEST) {
				fID_HOPS.insert({flit_id, INT_MAX});
			} else if (threshold_mode == ThresholdMode::LIMITED) {
				fID_HOPS.insert({flit_id, MAX_HOPS});
			} else if (threshold_mode == ThresholdMode::ALL) {
				fID_HOPS.insert({flit_id, INT_MAX / 2});
			} else {
				assert(false && "Unknown threshold mode");
			}
		}
	// This is not the first hop, classify the incoming channel type based on the previous router
	} else {
		in_ct = classify_channel(prev_router, cur);
	}

	// Get all valid first hops using threshold-aware path computation
	vector<int> first_hops = compute_next_hops(router_list, src_rid, cur, dst_router, prev_router, flit_id, in_ct);

	// Convert to result format: (port, vc_start, vc_end)
	vector<tuple<int, int, int>> result;
	for (int port : first_hops) {
		result.emplace_back(port, 0, n_vcs - 1);
	}
	// Decrement hop budget for this flit (unless the id is negative, which we use to allow arbitrary many hops)
	if (flit_id >= 0) {
		fID_HOPS[flit_id] -= 1;
	}
	return result;
}

void lturn_routing::increment_hop_budget(int flit_id) {
	if (fID_HOPS.count(flit_id)) {
	fID_HOPS[flit_id] += 1;
	}
}

// ================================================
// DEBUGGING PRINTS
// ================================================

void lturn_routing::print_mst_topology() const {

	std::cout << "\n=======================================\n";
	std::cout << "L-TURN ROUTING - MST TOPOLOGY\n";
	std::cout << "=======================================\n\n";

	std::cout << "Root Router: " << root << " (Level " << level[root]
						<< ", Width " << width[root] << ")\n\n";

	// Print BFS tree structure
	std::cout << "BFS Spanning Tree:\n";
	std::cout << "------------------\n";

	// Group routers by level
	std::map<int, std::vector<int>> routers_by_level;
	for (size_t i = 0; i < level.size(); ++i) {
		routers_by_level[level[i]].push_back(i);
	}

	for (const auto &[lvl, routers] : routers_by_level) {
		std::cout << "Level " << lvl << ": ";
		for (int rid : routers) {
			std::cout << rid << "(w:" << width[rid];
			if (tree_parent[rid] != -1) {
				std::cout << ", p:" << tree_parent[rid];
			}
			std::cout << ") ";
		}
		std::cout << "\n";
	}

	// Print channel classification matrix
	std::cout << "\nChannel Classifications:\n";
	std::cout << "------------------------\n";
	for (size_t u = 0; u < adj.size(); ++u) {
		std::cout << "Router " << u << " (L" << level[u] << ",W" << width[u]
							<< "): ";
		for (size_t v : adj[u]) {
			ChannelType ct = classify_channel(u, v);
			std::cout << v << "(";
			switch (ct) {
			case ChannelType::LU:
				std::cout << "LU";
				break;
			case ChannelType::LD:
				std::cout << "LD";
				break;
			case ChannelType::RU:
				std::cout << "RU";
				break;
			case ChannelType::RD:
				std::cout << "RD";
				break;
			}
			std::cout << ") ";
		}
		std::cout << "\n";
	}
	std::cout << "=======================================\n\n";
}

void lturn_routing::debug_print_state() const {
	std::cout << "\n=== L-TURN ROUTING DEBUG STATE ===\n";
	std::cout << "Initialized: " << (initialized ? "YES" : "NO") << "\n";
	std::cout << "Threshold mode: ";
	switch (threshold_mode) {
	case ThresholdMode::SHORTEST:
		std::cout << "SHORTEST";
		break;
	case ThresholdMode::BOUNDED:
		std::cout << "BOUNDED";
		break;
	case ThresholdMode::LIMITED:
		std::cout << "LIMITED";
		break;
	case ThresholdMode::ALL:
		std::cout << "ALL";
		break;
	}
	std::cout << "\n";
	if (threshold_mode == ThresholdMode::LIMITED) {
		std::cout << "MAX_HOPS: " << MAX_HOPS << "\n";
	}
}


string lturn_routing::channelTypeToString(ChannelType ct) const {
	switch (ct) {
	case ChannelType::LU:
		return "LU";
	case ChannelType::LD:
		return "LD";
	case ChannelType::RU:
		return "RU";
	case ChannelType::RD:
		return "RD";
	default:
		return "UNKNOWN";
	}
}
