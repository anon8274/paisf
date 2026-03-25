#include <filesystem>
#include <fstream>

#include "lash_tor_routing.hpp"
#include "topology_hash.hpp"
#include "../config_utils.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

lash_tor_routing::lash_tor_routing(const Configuration &config) : _config(config) {
	num_vcs = _config.GetInt("num_vcs");
    n_layers = num_vcs;             // We use one layer per VC, otherwise, in-order delivery is not guaranteed
    fallback_alg = "up_down";       // Default fallback algorithm (as described in the paper)
    repetition = _config.GetInt("repetition");
    cache_directory = _config.GetStr("path_for_cache");
    // Random seed
    if(config.GetStr("seed") == "time") {
      shuffle_seed = int(time(NULL));
    } else {
      shuffle_seed = config.GetInt("seed");
    }

}

void lash_tor_routing::initialize(
		const vector<map<int, map<int, tuple<int, int, int>>>> &router_list,
		int n_routers, int n_vcs) const {

	initialized = true;
	topo_n_routers = n_routers;

	build_node_maps(router_list, n_routers);
	build_links(router_list, n_routers);
	compute_prev_router_map(router_list);

	// assign normal layers to available VCs (need at least 1 VC for fallback)
	normal_layers = max(1, min(n_layers - 1, max(1, n_vcs - 1)));
	fallback_layer = normal_layers; // virtual layer id

	// dependency graphs for normal layers
	layer_cdg.assign(normal_layers + 1, {});
	layer_cdg_refcnt.assign(normal_layers + 1, {});
	active_balance_layers = normal_layers;
	fallback_promoted_for_balance = false;
	committed_paths.clear();

	compute_vc_ranges(n_vcs);

	// fallback routing
    if (fallback_alg == "up_down") {
        fallback_routing = make_unique<up_down_routing>(_config);
    } else if (fallback_alg == "left_right") {
        fallback_routing = make_unique<left_right_routing>(_config);
    } else if (fallback_alg == "lturn") {
        fallback_routing = make_unique<lturn_routing>(_config);
    } else {
        cerr << "Unknown fallback algorithm: " << fallback_alg << endl;
        assert(false);
    }

	// Enable dense stamping if size is reasonable
	num_channel_ids = n_routers * n_routers;
	// const size_t max_dense =
	//		 50 * 1000 * 1000;
	// dense_reach_enabled = (num_channel_ids > 0 && num_channel_ids <=
	// max_dense);
	dense_reach_enabled = true;
	if (dense_reach_enabled) {
		reach_stamp.assign(num_channel_ids, 0);
		reach_token = 1;
	} else {
		reach_stamp.clear();
	}


	// Compute hash of the topology
    size_t topology_hash = router_hash::hash_router_list(router_list);
    string rt_cache_file = cache_directory + "lash_tor_routing_table_" + to_string(topology_hash) + "_rep" + to_string(repetition) + "_" + to_string(n_layers) + "layers.bin";
    // Check if the routing tables for this topology and repetition are already cached
    if (filesystem::exists(rt_cache_file)) {
        // Load routing tables from cache
        LashTorRoutingTable rt;
        rt.load(rt_cache_file);
        routing_table = rt.routing_table;
        layer_table = rt.layer_table;
        fallback_paths = rt.fallback_paths;
        int n_fallback_false = 0;
        int n_fallback_true = 0;
        for (const auto &kv1 : fallback_paths) {
            for (const auto &kv2 : kv1.second) {
                if (kv2.second)
                    n_fallback_true++;
                else
                    n_fallback_false++;
            }
        }
        cout << "Loaded LASH-TOR routing tables from cache: " << rt_cache_file << endl;
        cout << "Number of fallback pairs: False=" << n_fallback_false << ", True=" << n_fallback_true << endl;
    } else {
		// Build list of (src,dst) pairs only among routers that have nodes
		vector<pair<int, int>> pairs;
		pairs.reserve(routers_with_nodes.size() * routers_with_nodes.size());
		for (int s : routers_with_nodes) {
			for (int d : routers_with_nodes) {
				if (s != d)
					pairs.emplace_back(s, d);
			}
		}

		// Randomize processing order
		mt19937 rng(shuffle_seed);
		shuffle(pairs.begin(), pairs.end(), rng);

		if (n_vcs == 1) {
			// Special policy for the 1-VC case:
			// try to place everything in the normal layer;
			// if one pair does not fit, send everything to fallback.
			build_one_vc_mode_or_all_fallback(pairs, n_routers);
		} else {
			for (auto [src, dst] : pairs) {
				DagInfo dag = build_shortest_path_dag(src, n_routers);
				if (dag.dist.empty() || dag.dist[dst] == INT_MAX) {
					fallback_paths[src][dst] = true;
					continue;
				}

				BestPath best = find_best_shortest_path_all(dag, src, dst);
				if (!best.found) {
					fallback_paths[src][dst] = true;
					continue;
				}

				commit_path(src, dst, best.path, best.hop_layers);
				committed_paths[src][dst] = best;
				fallback_paths[src][dst] = false;
			}
			maybe_promote_fallback_for_balance();
			balance_layer_loads();
		}
		// Store routing tables in cache for future runs (if not already stored by a different thread)
        if (!filesystem::exists(rt_cache_file)) {
            LashTorRoutingTable rt;
            rt.routing_table = routing_table;
            rt.layer_table   = layer_table;
            rt.fallback_paths = fallback_paths;
            rt.save(rt_cache_file);
            cout << "Saved LASH-TOR routing tables to cache: " << rt_cache_file << endl;
            int n_fallback_false = 0;
            int n_fallback_true = 0;
            for (const auto &kv1 : fallback_paths) {
                for (const auto &kv2 : kv1.second) {
                    if (kv2.second)
                        n_fallback_true++;
                    else
                        n_fallback_false++;
                }
            }
            cout << "Number of fallback pairs: False=" << n_fallback_false << ", True=" << n_fallback_true << endl;
        }
	}
	// Validate all paths
	// validate_tables(router_list, n_routers);
	// validate_cdg();
}

void lash_tor_routing::maybe_promote_fallback_for_balance() const {
	fallback_promoted_for_balance = false;
	active_balance_layers = normal_layers;

	// If any pair uses fallback, keep fallback reserved.
	for (const auto &src_kv : fallback_paths) {
		for (const auto &dst_kv : src_kv.second) {
			if (dst_kv.second) {
				return;
			}
		}
	}

	// No fallback pairs at all: promote fallback VC/layer for balancing.
	fallback_promoted_for_balance = true;
	active_balance_layers = normal_layers + 1;
}

void lash_tor_routing::build_node_maps(
		const vector<map<int, map<int, tuple<int, int, int>>>> &router_list,
		int n_routers) const {

	node_to_router.clear();
	routers_with_nodes.clear();
	router_has_nodes.assign(n_routers, 0);

	for (int rid = 0; rid < n_routers; ++rid) {
		auto it = router_list[0].find(rid);
		if (it == router_list[0].end())
			continue;
		bool any = false;
		for (const auto &n : it->second) {
			node_to_router[n.first] = rid;
			any = true;
		}
		if (any) {
			router_has_nodes[rid] = 1;
			routers_with_nodes.push_back(rid);
		}
	}
}

void lash_tor_routing::build_links(
		const vector<map<int, map<int, tuple<int, int, int>>>> &router_list,
		int n_routers) const {

	edge_ports.clear();
	adj.assign(n_routers, {});

	for (int u = 0; u < n_routers; ++u) {
		auto it = router_list[1].find(u);
		if (it == router_list[1].end())
			continue;

		for (const auto &e : it->second) {
			int v = e.first;
			int out_port = get<0>(e.second);
			edge_ports[u][v].push_back(out_port);
		}
	}

	for (int u = 0; u < n_routers; ++u) {
		auto it = edge_ports.find(u);
		if (it == edge_ports.end())
			continue;
		for (const auto &kv : it->second) {
			adj[u].push_back(kv.first);
		}
		sort(adj[u].begin(), adj[u].end());
		adj[u].erase(unique(adj[u].begin(), adj[u].end()), adj[u].end());
	}
}

void lash_tor_routing::compute_prev_router_map(
		const vector<map<int, map<int, tuple<int, int, int>>>> &router_list) const {

	prev_router_map.clear();
	for (const auto &u_entry : router_list[1]) {
		int u = u_entry.first;
		for (const auto &e : u_entry.second) {
			int v = e.first;
			int in_port_at_v = get<1>(e.second);
			prev_router_map[v][in_port_at_v] = u;
		}
	}
}

void lash_tor_routing::compute_vc_ranges(int n_vcs) const {
	// distribute normal VCs evenly across normal layers; remainder to fallback
	int L = normal_layers;
	int normal_vcs = max(1, n_vcs - 1); // try to leave at least 1 vc for fallback
	int base = normal_vcs / L;
	int rem = normal_vcs % L;

	layer_to_vc.assign(L + 1, VcRange{0, -1}); // +1 for fallback
	int cur = 0;
	for (int l = 0; l < L; ++l) {
		int take = base + (l < rem ? 1 : 0);
		if (take < 1)
			take = 1;
		layer_to_vc[l] = {cur, min(n_vcs - 1, cur + take - 1)};
		cur += take;
	}

	fallback_vc_begin = min(cur, n_vcs - 1);
	layer_to_vc[L] = {fallback_vc_begin, n_vcs - 1};
}

vector<int> lash_tor_routing::get_ports(int u, int v) const {
	auto it = edge_ports.find(u);
	if (it == edge_ports.end())
		return {};
	auto jt = it->second.find(v);
	if (jt == it->second.end())
		return {};
	return jt->second;
}

bool lash_tor_routing::reachable_channel_with_temp(
		int layer, int start_ch, int target_ch,
		const unordered_map<int, unordered_set<int>> &temp_edges) const {

	if (start_ch == target_ch)
		return true;
	if (layer < 0 || layer >= (int)layer_cdg.size())
		return false;

	// Dense-stamp BFS
	if (dense_reach_enabled) {
		reach_token++;
		if (reach_token == 0) {
			fill(reach_stamp.begin(), reach_stamp.end(), 0);
			reach_token = 1;
		}

		deque<int> dq;
		dq.push_back(start_ch);
		if ((size_t)start_ch < reach_stamp.size()) {
			reach_stamp[start_ch] = reach_token;
		}

		while (!dq.empty()) {
			int u = dq.front();
			dq.pop_front();

			// Explore real CDG edges
			auto it_real = layer_cdg[layer].find(u);
			if (it_real != layer_cdg[layer].end()) {
				for (int v : it_real->second) {
					if (v == target_ch)
						return true;
					if ((size_t)v < reach_stamp.size() && reach_stamp[v] != reach_token) {
						reach_stamp[v] = reach_token;
						dq.push_back(v);
					}
				}
			}

			// Explore temporary edges for this candidate path
			auto it_temp = temp_edges.find(u);
			if (it_temp != temp_edges.end()) {
				for (int v : it_temp->second) {
					if (v == target_ch)
						return true;
					if ((size_t)v < reach_stamp.size() && reach_stamp[v] != reach_token) {
						reach_stamp[v] = reach_token;
						dq.push_back(v);
					}
				}
			}
		}
		return false;
	}

	// Sparse fallback
	deque<int> dq;
	unordered_set<int> vis;
	vis.reserve(1024);

	dq.push_back(start_ch);
	vis.insert(start_ch);

	while (!dq.empty()) {
		int u = dq.front();
		dq.pop_front();

		auto it_real = layer_cdg[layer].find(u);
		if (it_real != layer_cdg[layer].end()) {
			for (int v : it_real->second) {
				if (v == target_ch)
					return true;
				if (!vis.count(v)) {
					vis.insert(v);
					dq.push_back(v);
				}
			}
		}

		auto it_temp = temp_edges.find(u);
		if (it_temp != temp_edges.end()) {
			for (int v : it_temp->second) {
				if (v == target_ch)
					return true;
				if (!vis.count(v)) {
					vis.insert(v);
					dq.push_back(v);
				}
			}
		}
	}

	return false;
}

bool lash_tor_routing::reachable_channel(int layer, int start_ch,
																				 int target_ch) const {
	if (start_ch == target_ch)
		return true;

	if (layer < 0 || layer >= (int)layer_cdg.size())
		return false;

	// Dense-stamp BFS
	if (dense_reach_enabled) {
		// token bump
		reach_token++;
		if (reach_token == 0) {
			fill(reach_stamp.begin(), reach_stamp.end(), 0);
			reach_token = 1;
		}

		deque<int> dq;
		dq.push_back(start_ch);
		if ((size_t)start_ch < reach_stamp.size())
			reach_stamp[start_ch] = reach_token;

		while (!dq.empty()) {
			int u = dq.front();
			dq.pop_front();
			auto it = layer_cdg[layer].find(u);
			if (it == layer_cdg[layer].end())
				continue;

			for (int v : it->second) {
				if (v == target_ch)
					return true;
				if ((size_t)v >= reach_stamp.size())
					continue;
				if (reach_stamp[v] != reach_token) {
					reach_stamp[v] = reach_token;
					dq.push_back(v);
				}
			}
		}
		return false;
	}

	// Fallback sparse visited set
	deque<int> dq;
	unordered_set<int> vis;
	dq.push_back(start_ch);
	vis.insert(start_ch);

	while (!dq.empty()) {
		int u = dq.front();
		dq.pop_front();
		auto it = layer_cdg[layer].find(u);
		if (it == layer_cdg[layer].end())
			continue;

		for (int v : it->second) {
			if (v == target_ch)
				return true;
			if (!vis.count(v)) {
				vis.insert(v);
				dq.push_back(v);
			}
		}
	}
	return false;
}

bool lash_tor_routing::would_create_cycle_ch(int layer, int from_ch,
																						 int to_ch) const {
	// adding from_ch -> to_ch creates a cycle iff to_ch can already reach from_ch
	return reachable_channel(layer, to_ch, from_ch);
}

void lash_tor_routing::add_cdg_edge_ref(int layer, int from_ch,
																				int to_ch) const {
	int &cnt = layer_cdg_refcnt[layer][from_ch][to_ch];
	cnt++;
	if (cnt == 1) {
		layer_cdg[layer][from_ch].insert(to_ch);
	}
}

void lash_tor_routing::remove_cdg_edge_ref(int layer, int from_ch,
																					 int to_ch) const {
	auto it_from = layer_cdg_refcnt[layer].find(from_ch);
	if (it_from == layer_cdg_refcnt[layer].end())
		return;

	auto it_to = it_from->second.find(to_ch);
	if (it_to == it_from->second.end())
		return;

	it_to->second--;
	if (it_to->second <= 0) {
		it_from->second.erase(it_to);

		auto it_graph = layer_cdg[layer].find(from_ch);
		if (it_graph != layer_cdg[layer].end()) {
			it_graph->second.erase(to_ch);
			if (it_graph->second.empty()) {
				layer_cdg[layer].erase(it_graph);
			}
		}
	}

	if (it_from->second.empty()) {
		layer_cdg_refcnt[layer].erase(it_from);
	}
}

// Shortest path DAG
lash_tor_routing::DagInfo
lash_tor_routing::build_shortest_path_dag(int src, int n_routers) const {
	DagInfo dag;
	dag.dist.assign(n_routers, INT_MAX);
	dag.pred.assign(n_routers, {});

	queue<int> q;
	dag.dist[src] = 0;
	q.push(src);

	while (!q.empty()) {
		int u = q.front();
		q.pop();
		int du = dag.dist[u];

		for (int v : adj[u]) {
			if (dag.dist[v] == INT_MAX) {
				dag.dist[v] = du + 1;
				dag.pred[v].push_back(u);
				q.push(v);
			} else if (dag.dist[v] == du + 1) {
				dag.pred[v].push_back(u);
			}
		}
	}

	// deterministic predecessor order
	for (auto &pv : dag.pred) {
		sort(pv.begin(), pv.end());
		pv.erase(unique(pv.begin(), pv.end()), pv.end());
	}

	return dag;
}

// All shortest paths search
lash_tor_routing::BestPath
lash_tor_routing::find_best_shortest_path_all(const DagInfo &dag, int src,
																							int dst) const {
	BestPath best;
	best.found = false;
	best.layers_used = INT_MAX;

	vector<int> rev_path;
	rev_path.reserve((size_t)dag.dist[dst] + 1);

	dfs_all_shortest_paths(dst, src, dag, rev_path, best, dst);
	return best;
}

void lash_tor_routing::dfs_all_shortest_paths(int node, int src,
																							const DagInfo &dag,
																							vector<int> &rev_path,
																							BestPath &best, int dst) const {

	rev_path.push_back(node);

	if (node == src) {
		vector<int> path = rev_path;
		reverse(path.begin(), path.end()); // src..dst

		int used = INT_MAX;
		vector<int> hop_layers;
		if (assign_layers_for_path(path, used, hop_layers)) {
			if (!best.found || used < best.layers_used) {
				best.found = true;
				best.layers_used = used;
				best.path = std::move(path);
				best.hop_layers = std::move(hop_layers);
				// early exit if we found a 1-layer solution
				if (best.layers_used == 1) {
					rev_path.pop_back();
					return;
				}
			}
		}

		rev_path.pop_back();
		return;
	}

	// early stop if already perfect
	if (best.found && best.layers_used == 1) {
		rev_path.pop_back();
		return;
	}

	for (int p : dag.pred[node]) {
		dfs_all_shortest_paths(p, src, dag, rev_path, best, dst);
		if (best.found && best.layers_used == 1)
			break;
	}

	rev_path.pop_back();
}

bool lash_tor_routing::build_one_vc_mode_or_all_fallback(
		const vector<pair<int, int>> &pairs, int n_routers) const {

	// Clean state for a fresh attempt
	routing_table.clear();
	layer_table.clear();
	fallback_paths.clear();
	committed_paths.clear();
	layer_cdg.assign(normal_layers, {});
	layer_cdg_refcnt.assign(normal_layers, {});

	for (auto [src, dst] : pairs) {
		DagInfo dag = build_shortest_path_dag(src, n_routers);
		if (dag.dist.empty() || dag.dist[dst] == INT_MAX) {
			// Fail whole attempt -> all fallback
			routing_table.clear();
			layer_table.clear();
			committed_paths.clear();
			layer_cdg.assign(normal_layers, {});
			layer_cdg_refcnt.assign(normal_layers, {});

			for (auto [s, d] : pairs) {
				fallback_paths[s][d] = true;
			}
			return false;
		}

		BestPath best = find_best_shortest_path_all(dag, src, dst);
		if (!best.found || best.layers_used != 1) {
			// If any path cannot stay entirely in the single normal layer,
			// force everything to fallback.
			routing_table.clear();
			layer_table.clear();
			committed_paths.clear();
			layer_cdg.assign(normal_layers, {});
			layer_cdg_refcnt.assign(normal_layers, {});

			for (auto [s, d] : pairs) {
				fallback_paths[s][d] = true;
			}
			return false;
		}

		commit_path(src, dst, best.path, best.hop_layers);
		committed_paths[src][dst] = best;
		fallback_paths[src][dst] = false;
	}

	return true;
}

bool lash_tor_routing::assign_layers_for_path(
		const vector<int> &path, int &layers_used_out,
		vector<int> &hop_layers_out) const {

	if (path.size() < 2)
		return false;

	const size_t hops = path.size() - 1;
	hop_layers_out.assign(hops, 0);

	// Temporary CDG edges introduced by THIS candidate path only.
	// temp_cdg[layer][from_ch] = set of to_ch
	vector<unordered_map<int, unordered_set<int>>> temp_cdg(normal_layers);

	int cur_layer = 0;
	int max_layer_used = 0;

	int prev_ch = -1;
	int prev_layer = -1;

	for (size_t i = 0; i < hops; ++i) {
		int u = path[i];
		int v = path[i + 1];
		int cur_ch = chan_id(u, v);

		int chosen = -1;

		for (int l = cur_layer; l < normal_layers; ++l) {
			bool ok = true;

			// If previous hop is in the same layer, then we'd add CDG edge prev_ch ->
			// cur_ch. This must be checked against:
			//	 real layer_cdg[l] + temp_cdg[l]
			if (i >= 1 && l == prev_layer) {
				// Adding prev_ch -> cur_ch creates a cycle iff cur_ch can reach prev_ch
				// in the union of real and temporary CDG edges.
				if (reachable_channel_with_temp(l, cur_ch, prev_ch, temp_cdg[l])) {
					ok = false;
				}
			}

			if (ok) {
				chosen = l;
				break;
			}
		}

		if (chosen < 0) {
			return false; // needs fallback
		}

		hop_layers_out[i] = chosen;
		cur_layer = chosen;
		if (chosen > max_layer_used)
			max_layer_used = chosen;

		// If this hop continues in the same layer as previous hop,
		// record the new temporary dependency edge immediately.
		if (i >= 1 && chosen == prev_layer) {
			temp_cdg[chosen][prev_ch].insert(cur_ch);
		}

		prev_ch = cur_ch;
		prev_layer = chosen;
	}

	layers_used_out = max_layer_used + 1;
	return true;
}

void lash_tor_routing::commit_path(int src, int dst, const vector<int> &path,
																	 const vector<int> &hop_layers) const {

	const size_t hops = path.size() - 1;

	// Store next hop and layer at each router on the path
	for (size_t i = 0; i < hops; ++i) {
		int u = path[i];
		int v = path[i + 1];
		int l = hop_layers[i];
		routing_table[src][u][dst] = v;
		layer_table[src][u][dst] = l;
	}

	// Add CDG dependencies for consecutive channels
	for (size_t i = 1; i < hops; ++i) {
		if (hop_layers[i - 1] != hop_layers[i])
			continue; // transition breaks dependency
		int a = path[i - 1];
		int b = path[i];
		int c = path[i + 1];
		int ch1 = chan_id(a, b);
		int ch2 = chan_id(b, c);
		int l = hop_layers[i];
		add_cdg_edge_ref(l, ch1, ch2);
	}
}

vector<int> lash_tor_routing::compute_layer_usage() const {
	vector<int> usage(active_balance_layers, 0);

	for (const auto &src_kv : committed_paths) {
		for (const auto &dst_kv : src_kv.second) {
			const BestPath &bp = dst_kv.second;
			for (int l : bp.hop_layers) {
				if (0 <= l && l < active_balance_layers) {
					usage[l]++;
				}
			}
		}
	}

	return usage;
}

bool lash_tor_routing::try_move_segment_up(int src, int dst, BestPath &bp,
																					 int seg_begin_hop, int seg_end_hop,
																					 int target_layer,
																					 vector<int> &layer_usage) const {

	const int old_layer = bp.hop_layers[seg_begin_hop];
	if (target_layer <= old_layer)
		return false;

	const int hops = (int)bp.hop_layers.size();

	// Segment must be maximal same-layer segment
	if (seg_begin_hop > 0 && bp.hop_layers[seg_begin_hop - 1] == old_layer)
		return false;
	if (seg_end_hop + 1 < hops && bp.hop_layers[seg_end_hop + 1] == old_layer)
		return false;

	// Monotonicity guard:
	// previous layer <= target_layer always holds because previous <= old_layer <
	// target_layer. Need next layer >= target_layer if there is a next segment.
	if (seg_end_hop + 1 < hops) {
		int next_layer = bp.hop_layers[seg_end_hop + 1];
		if (next_layer < target_layer)
			return false;
	}

	// temp edges that would be added to target layer
	unordered_map<int, unordered_set<int>> temp_add;

	// Internal dependencies inside moved segment
	// Consecutive hops inside same target layer create CDG edges
	for (int i = seg_begin_hop + 1; i <= seg_end_hop; ++i) {
		int a = bp.path[i - 1];
		int b = bp.path[i];
		int c = bp.path[i + 1];

		int ch1 = chan_id(a, b);
		int ch2 = chan_id(b, c);

		// Check against real target CDG + temporary additions
		if (reachable_channel_with_temp(target_layer, ch2, ch1, temp_add)) {
			return false;
		}
		temp_add[ch1].insert(ch2);
	}

	// Boundary on the right:
	// if next segment already uses target_layer, moving this segment up merges
	// them, so add dependency from last moved hop to next hop.
	if (seg_end_hop + 1 < hops &&
			bp.hop_layers[seg_end_hop + 1] == target_layer) {
		int a = bp.path[seg_end_hop];
		int b = bp.path[seg_end_hop + 1];
		int c = bp.path[seg_end_hop + 2];

		int ch1 = chan_id(a, b);
		int ch2 = chan_id(b, c);

		if (reachable_channel_with_temp(target_layer, ch2, ch1, temp_add)) {
			return false;
		}
		temp_add[ch1].insert(ch2);
	}

	// Safe: remove old dependencies from old layer
	for (int i = seg_begin_hop + 1; i <= seg_end_hop; ++i) {
		int a = bp.path[i - 1];
		int b = bp.path[i];
		int c = bp.path[i + 1];

		int ch1 = chan_id(a, b);
		int ch2 = chan_id(b, c);
		remove_cdg_edge_ref(old_layer, ch1, ch2);
	}

	// Add new dependencies to target layer
	for (const auto &kv : temp_add) {
		int from_ch = kv.first;
		for (int to_ch : kv.second) {
			add_cdg_edge_ref(target_layer, from_ch, to_ch);
		}
	}

	// Update hop layers
	const int moved_hops = seg_end_hop - seg_begin_hop + 1;
	for (int i = seg_begin_hop; i <= seg_end_hop; ++i) {
		bp.hop_layers[i] = target_layer;
	}

	// Update routing layer table for the moved hops
	for (int i = seg_begin_hop; i <= seg_end_hop; ++i) {
		int u = bp.path[i];
		layer_table[src][u][dst] = target_layer;
	}

	layer_usage[old_layer] -= moved_hops;
	layer_usage[target_layer] += moved_hops;

	return true;
}

void lash_tor_routing::balance_layer_loads() const {
	if (active_balance_layers <= 1)
		return;

	vector<int> usage = compute_layer_usage();

	// A few passes is enough; keep it conservative
	for (int pass = 0; pass < 4; ++pass) {
		int total = 0;
		for (int x : usage)
			total += x;
		if (total == 0)
			return;

		double avg = double(total) / double(active_balance_layers);
		bool moved_any = false;

		// Find layers sorted by usage descending
		vector<int> order(active_balance_layers);
		iota(order.begin(), order.end(), 0);
		sort(order.begin(), order.end(),
				 [&](int a, int b) { return usage[a] > usage[b]; });

		for (int over_layer : order) {
			if (usage[over_layer] <= avg * 1.10)
				continue; // not overloaded enough

			// Try to move segments from this layer to a lighter higher layer
			for (auto &src_kv : committed_paths) {
				int src = src_kv.first;
				for (auto &dst_kv : src_kv.second) {
					int dst = dst_kv.first;
					BestPath &bp = dst_kv.second;

					const int hops = (int)bp.hop_layers.size();
					int i = 0;
					while (i < hops) {
						if (bp.hop_layers[i] != over_layer) {
							++i;
							continue;
						}

						int j = i;
						while (j + 1 < hops && bp.hop_layers[j + 1] == over_layer)
							++j;

						// Next segment layer bounds the target to preserve monotonicity
						int max_target = active_balance_layers - 1;
						if (j + 1 < hops) {
							max_target = bp.hop_layers[j + 1];
						}

						// Try the lightest admissible higher layer
						int best_target = -1;
						int best_usage = INT_MAX;
						for (int t = over_layer + 1; t <= max_target; ++t) {
							if (usage[t] < best_usage) {
								best_usage = usage[t];
								best_target = t;
							}
						}

						if (best_target != -1 &&
								usage[best_target] + (j - i + 1) < usage[over_layer]) {
							if (try_move_segment_up(src, dst, bp, i, j, best_target, usage)) {
								moved_any = true;
								if (usage[over_layer] <= avg * 1.10)
									break;
							}
						}

						i = j + 1;
					}

					if (usage[over_layer] <= avg * 1.10)
						break;
				}

				if (usage[over_layer] <= avg * 1.10)
					break;
			}
		}

		if (!moved_any)
			break;
	}
}

vector<tuple<int, int, int>> lash_tor_routing::route(vector<map<int, map<int, tuple<int, int, int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const {

	if (!initialized) {
		initialize(router_list, n_routers, n_vcs);
	}

	// Ejection if at destination router
	if (cur_rid == dst_rid) {
		if (router_list[0].count(cur_rid)) {
			for (const auto &node_entry : router_list[0].at(cur_rid)) {
				if (node_entry.first == dst_nid) {
					int port = get<0>(node_entry.second);
					return {make_tuple(port, 0, n_vcs - 1)};
				}
			}
		}
		return {};
	}

	// fallback for this pair
	auto fb1 = fallback_paths.find(src_rid);
	if (fb1 != fallback_paths.end()) {
		auto fb2 = fb1->second.find(dst_rid);
		if (fb2 != fb1->second.end() && fb2->second) {
			auto res = fallback_routing->route(router_list, n_routers, n_nodes, n_vcs, src_rid,	cur_rid, cur_vc, dst_nid, dst_rid, in_port, flit_id);
			vector<tuple<int, int, int>> out;
			out.reserve(res.size());
			for (auto &t : res)
				out.emplace_back(get<0>(t), fallback_vc_begin, n_vcs - 1);
			return out;
		}
	}

	// Lookup next hop + layer
	int next = -1;
	int lyr = -1;

	auto s1 = routing_table.find(src_rid);
	if (s1 != routing_table.end()) {
		auto s2 = s1->second.find(cur_rid);
		if (s2 != s1->second.end()) {
			auto s3 = s2->second.find(dst_rid);
			if (s3 != s2->second.end())
				next = s3->second;
		}
	}

	auto l1 = layer_table.find(src_rid);
	if (l1 != layer_table.end()) {
		auto l2 = l1->second.find(cur_rid);
		if (l2 != l1->second.end()) {
			auto l3 = l2->second.find(dst_rid);
			if (l3 != l2->second.end())
				lyr = l3->second;
		}
	}

	if (next < 0 || lyr < 0) {
		// missing entry => fallback
		auto res = fallback_routing->route(router_list, n_routers, n_nodes, n_vcs, src_rid, cur_rid, cur_vc, dst_nid, dst_rid, in_port, flit_id);
		vector<tuple<int, int, int>> out;
		out.reserve(res.size());
		for (auto &t : res)
			out.emplace_back(get<0>(t), fallback_vc_begin, n_vcs - 1);
		return out;
	}

	auto ports = get_ports(cur_rid, next);
	if (ports.empty()) {
		cerr << "LASH_TOR ERROR: no port from " << cur_rid << " to " << next
				 << endl;
		assert(false);
	}

	// Layer -> VC range
	if (lyr < 0 || lyr >= (int)layer_to_vc.size()) {
		vector<tuple<int, int, int>> out;
		for (int p : ports)
			out.emplace_back(p, fallback_vc_begin, n_vcs - 1);
		return out;
	}

	auto vr = layer_to_vc[lyr];
	int vc_b = max(0, vr.begin);
	int vc_e = min(n_vcs - 1, vr.end);
	if (vc_e < vc_b) {
		vc_b = fallback_vc_begin;
		vc_e = n_vcs - 1;
	}

	vector<tuple<int, int, int>> result;
	result.reserve(ports.size());
	for (int p : ports)
		result.emplace_back(p, vc_b, vc_e);
	return result;
}
bool lash_tor_routing::has_cycle_in_cdg_layer(int layer) const {
	unordered_map<int, int> state; // 0=unseen, 1=visiting, 2=done

	function<bool(int)> dfs = [&](int u) -> bool {
		state[u] = 1;
		auto it = layer_cdg[layer].find(u);
		if (it != layer_cdg[layer].end()) {
			for (int v : it->second) {
				if (state[v] == 1)
					return true;
				if (state[v] == 0 && dfs(v))
					return true;
			}
		}
		state[u] = 2;
		return false;
	};

	for (const auto &kv : layer_cdg[layer]) {
		int u = kv.first;
		if (state[u] == 0) {
			if (dfs(u))
				return true;
		}
	}
	return false;
}

void lash_tor_routing::validate_cdg() const {
	std::cout << "\n=== LASH-TOR CDG validation ===\n";

	int used_normal_layers = 0;
	int highest_used_normal_layer = -1;

	// If fallback was promoted, this is the highest layer that may act as normal
	int max_checked_layer = active_balance_layers - 1;

	for (int l = 0; l <= max_checked_layer; ++l) {
		long long edge_count = 0;
		for (const auto &kv : layer_cdg[l]) {
			edge_count += kv.second.size();
		}

		bool cyc = has_cycle_in_cdg_layer(l);

		bool is_promoted_fallback_layer =
				fallback_promoted_for_balance && (l == fallback_layer);

		bool is_used_as_normal = (edge_count > 0);

		if (is_used_as_normal) {
			used_normal_layers++;
			highest_used_normal_layer = l;
		}

		std::cout << "Layer " << l;
		if (is_promoted_fallback_layer) {
			std::cout << " (promoted fallback)";
		} else if (l == fallback_layer) {
			std::cout << " (fallback)";
		}

		std::cout << ": CDG edges=" << edge_count
							<< ", cycle=" << (cyc ? "YES" : "NO") << "\n";
	}

	std::cout << "\nConfigured normal layers: " << normal_layers << "\n";
	std::cout << "Fallback promoted for balancing: "
						<< (fallback_promoted_for_balance ? "YES" : "NO") << "\n";
	std::cout << "Active balancing layers: " << active_balance_layers << "\n";
	std::cout << "Actually used normal layers: " << used_normal_layers << "\n";

	if (highest_used_normal_layer >= 0) {
		std::cout << "Highest used normal layer: " << highest_used_normal_layer
							<< "\n";
	} else {
		std::cout << "Highest used normal layer: none\n";
	}

	std::cout << "===============================\n\n";
}

void lash_tor_routing::validate_tables(
		const vector<map<int, map<int, tuple<int, int, int>>>> &router_list,
		int n_routers) const {

	vector<vector<int>> adj_local(n_routers);
	for (int u = 0; u < n_routers; ++u) {
		auto it = router_list[1].find(u);
		if (it == router_list[1].end())
			continue;
		for (const auto &e : it->second) {
			int v = e.first;
			adj_local[u].push_back(v);
		}
		sort(adj_local[u].begin(), adj_local[u].end());
		adj_local[u].erase(unique(adj_local[u].begin(), adj_local[u].end()),
											 adj_local[u].end());
	}

	auto bfs_dist = [&](int src, int dst) -> int {
		vector<int> dist(n_routers, INT_MAX);
		queue<int> q;
		dist[src] = 0;
		q.push(src);
		while (!q.empty()) {
			int u = q.front();
			q.pop();
			if (u == dst)
				return dist[u];
			for (int v : adj_local[u]) {
				if (dist[v] == INT_MAX) {
					dist[v] = dist[u] + 1;
					q.push(v);
				}
			}
		}
		return INT_MAX;
	};

	int total_pairs = 0;
	int fallback_pairs = 0;
	int missing_entry = 0;
	int loops_detected = 0;
	int nonminimal = 0;
	int layer_decrease = 0;

	vector<char> has_nodes(n_routers, 0);
	for (const auto &kv : node_to_router) {
		int rid = kv.second;
		if (0 <= rid && rid < n_routers)
			has_nodes[rid] = 1;
	}

	for (int src = 0; src < n_routers; ++src) {
		if (!has_nodes[src])
			continue;

		for (int dst = 0; dst < n_routers; ++dst) {
			if (src == dst)
				continue;
			if (!has_nodes[dst])
				continue;

			total_pairs++;

			// fallback
			if (fallback_paths.count(src) && fallback_paths.at(src).count(dst) &&
					fallback_paths.at(src).at(dst)) {
				fallback_pairs++;
				continue;
			}

			// must exist
			if (!routing_table.count(src) || !routing_table.at(src).count(src) ||
					!routing_table.at(src).at(src).count(dst)) {
				missing_entry++;
				continue;
			}

			// Walk the path from src to dst
			unordered_set<int> visited;
			visited.reserve((size_t)n_routers);

			int cur = src;
			int hops = 0;
			int last_layer = -1;
			bool bad = false;

			while (cur != dst) {
				if (visited.count(cur)) {
					loops_detected++;
					bad = true;
					break;
				}
				visited.insert(cur);

				// next hop lookup
				auto &rt_src = routing_table.at(src);
				if (!rt_src.count(cur) || !rt_src.at(cur).count(dst)) {
					missing_entry++;
					bad = true;
					break;
				}
				int nxt = rt_src.at(cur).at(dst);

				// layer lookup
				auto &lt_src = layer_table.at(src);
				if (!lt_src.count(cur) || !lt_src.at(cur).count(dst)) {
					missing_entry++;
					bad = true;
					break;
				}
				int lyr = lt_src.at(cur).at(dst);

				if (last_layer != -1 && lyr < last_layer) {
					layer_decrease++;
					bad = true;
					break;
				}
				last_layer = lyr;

				// prevent infinite walks
				hops++;
				if (hops > n_routers) {
					loops_detected++;
					bad = true;
					break;
				}

				cur = nxt;
			}

			if (bad)
				continue;

			int d = bfs_dist(src, dst);
			if (d == INT_MAX) {
				nonminimal++;
			} else if (hops != d) {
				nonminimal++;
			}
		}
	}

	// Print summary
	cout << "\n=== LASH-TOR validation summary ===\n";
	cout << "Validated pairs (routers with nodes only): " << total_pairs << "\n";
	cout << "Fallback pairs: " << fallback_pairs << " ("
			 << (total_pairs ? (100.0 * fallback_pairs / total_pairs) : 0.0)
			 << "%)\n";
	cout << "Missing table entries: " << missing_entry << "\n";
	cout << "Loops detected: " << loops_detected << "\n";
	cout << "Non-minimal hopcount pairs: " << nonminimal << "\n";
	cout << "Layer decreases detected: " << layer_decrease << "\n";
	cout << "==================================\n\n";
}
