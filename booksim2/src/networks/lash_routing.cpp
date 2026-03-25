#include "lash_routing.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <deque>
#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

lash_routing::lash_routing(const Configuration &config) : _config(config) {
  // Configure number of layers, the fallback alg and if fallback is enabled
  // n_layers = _config.GetInt("lash_layers");
  n_layers = _config.GetInt("num_vcs");
  // string fallback = _config.GetStr("lash_fallback");
  std::string fallback = "true";
  // fallback_alg = _config.GetStr("lash_fallback_alg");
  fallback_alg = "up_down";
  if (fallback == "true") {
    lash_fallback = true;
  } else {
    lash_fallback = false;
  }
  // string debug_mode_str = _config.GetStr("debug_mode");
  std::string debug_mode_str = "true";
  if (debug_mode_str == "true") {
    debug_mode = true;
  } else {
    debug_mode = false;
  }
}

void lash_routing::initialize(
    const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
        &router_list,
    int n_routers, int n_vcs) const {

  this->n_routers = n_routers;

  int vcs_per_layer = n_vcs / n_layers;
  if (vcs_per_layer < 1) {
    vcs_per_layer = 1;
    n_layers = n_vcs;
    std::cout << "Adjusted the number of layers to not be larger than number "
                 "of VCs"
              << std::endl;
  }

  // Build node to router mapping
  node_to_router.clear();
  for (int rid = 0; rid < n_routers; ++rid) {
    if (router_list[0].count(rid)) {
      for (const auto &node_entry : router_list[0].at(rid)) {
        node_to_router[node_entry.first] = rid;
      }
    }
  }

  // Build adjacency and edge ports
  adj.clear();
  edge_ports.clear();
  adj.resize(n_routers);
  edge_ports.resize(n_routers);

  for (int u = 0; u < n_routers; ++u) {
    if (router_list[1].count(u)) {
      for (const auto &e : router_list[1].at(u)) {
        int v = e.first;
        int out_port = std::get<0>(e.second);
        adj[u].push_back(v);
        edge_ports[u][v].push_back(out_port);
      }
    }
  }

  if (debug_mode) {
    build_lash_tables_debug();
  }

  compute_prev_router_map(router_list, n_routers);
  build_lash_tables();

  // initialize fallback_routing used on the last vLayer
  if (fallback_alg == "up_down") {
    fallback_routing = std::make_unique<up_down_routing>(_config);
  } else if (fallback_alg == "left_right") {
    fallback_routing = std::make_unique<left_right_routing>(_config);
  } else if (fallback_alg == "lturn") {
    fallback_routing = std::make_unique<lturn_routing>(_config);
  }

  initialized = true;

  // print_layer_statistics();
}

std::vector<int> lash_routing::shortest_path(int src, int dst) const {
  // BFS to find one shortest path
  if (src == dst)
    return {};

  std::vector<int> dist(n_routers, INT_MAX);
  std::vector<int> prev(n_routers, -1);
  std::queue<int> q;

  dist[src] = 0;
  q.push(src);

  while (!q.empty()) {
    int cur = q.front();
    q.pop();

    if (cur == dst)
      break;

    for (int nei : adj[cur]) {
      if (dist[nei] == INT_MAX) {
        dist[nei] = dist[cur] + 1;
        prev[nei] = cur;
        q.push(nei);
      }
    }
  }

  // Backtrack to reconstruct full path from src to dst
  std::vector<int> path;
  if (prev[dst] != -1) {
    int cur = dst;
    while (cur != src) {
      path.push_back(cur);
      cur = prev[cur];
    }
    path.push_back(src);
    std::reverse(path.begin(), path.end());
  }

  return path;
}

// Directed-channel id for edge (u -> v)
inline int ChanId(int u, int v, int n_routers) { return u * n_routers + v; }

// Reachability in a single real CDG
bool ReachableChannel(
    const std::unordered_map<int, std::unordered_set<int>> &cdg, int start_ch,
    int target_ch) {
  if (start_ch == target_ch)
    return true;

  std::deque<int> dq;
  std::unordered_set<int> vis;
  dq.push_back(start_ch);
  vis.insert(start_ch);

  while (!dq.empty()) {
    int u = dq.front();
    dq.pop_front();

    auto it = cdg.find(u);
    if (it == cdg.end())
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

// Reachability in real CDG + temporary edges from the current candidate path
bool ReachableChannelWithTemp(
    const std::unordered_map<int, std::unordered_set<int>> &real_cdg,
    const std::unordered_map<int, std::unordered_set<int>> &temp_cdg,
    int start_ch, int target_ch) {
  if (start_ch == target_ch)
    return true;

  std::deque<int> dq;
  std::unordered_set<int> vis;
  dq.push_back(start_ch);
  vis.insert(start_ch);

  while (!dq.empty()) {
    int u = dq.front();
    dq.pop_front();

    auto it_real = real_cdg.find(u);
    if (it_real != real_cdg.end()) {
      for (int v : it_real->second) {
        if (v == target_ch)
          return true;
        if (!vis.count(v)) {
          vis.insert(v);
          dq.push_back(v);
        }
      }
    }

    auto it_temp = temp_cdg.find(u);
    if (it_temp != temp_cdg.end()) {
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

// Kept for compatibility with the existing header. No longer used by the build.
bool lash_routing::creates_cycle(const std::vector<std::set<int>> &deps,
                                 int from, int to) const {
  if (from == to)
    return true;

  std::vector<bool> visited(n_routers, false);
  std::stack<int> stack;
  stack.push(to);

  while (!stack.empty()) {
    int current = stack.top();
    stack.pop();

    if (current == from) {
      return true;
    }

    if (!visited[current]) {
      visited[current] = true;
      for (int next : deps[current]) {
        if (!visited[next]) {
          stack.push(next);
        }
      }
    }
  }

  return false;
}

void lash_routing::build_lash_tables_debug() const {
  int n_layers_debug = 100;

  // Initialize structures
  layer_assignment.assign(n_routers, std::vector<int>(n_routers, -1));

  // Routing tables are [layer][current][destination] = next_hop
  routing_tables.assign(n_layers_debug,
                        std::vector<std::vector<int>>(
                            n_routers, std::vector<int>(n_routers, -1)));

  // Channel dependency graph per layer:
  // node = channel (u->v), edge = (u->v) -> (v->w)
  std::vector<std::unordered_map<int, std::unordered_set<int>>> layer_cdg(
      n_layers_debug);

  // Process all source-destination pairs
  std::vector<std::pair<int, int>> src_dst_pairs;
  for (int src = 0; src < n_routers; ++src) {
    for (int dst = 0; dst < n_routers; ++dst) {
      if (src != dst) {
        src_dst_pairs.push_back({src, dst});
      }
    }
  }

  highest_layer = 0;

  for (const auto &[src, dst] : src_dst_pairs) {
    std::vector<int> path = shortest_path(src, dst);
    if (path.empty())
      continue;

    bool assigned = false;
    bool cycle = false;
    int possible_layer = 0;

    for (int layer = 0; layer < n_layers_debug; ++layer) {
      cycle = false;

      // Temporary CDG edges introduced by this candidate path
      std::unordered_map<int, std::unordered_set<int>> temp_cdg;

      // A path with < 3 routers creates no channel-dependency edges
      for (size_t i = 1; i + 1 < path.size(); ++i) {
        int a = path[i - 1];
        int b = path[i];
        int c = path[i + 1];

        int ch1 = ChanId(a, b, n_routers);
        int ch2 = ChanId(b, c, n_routers);

        // Adding ch1 -> ch2 creates a cycle iff ch2 can already reach ch1
        if (ReachableChannelWithTemp(layer_cdg[layer], temp_cdg, ch2, ch1)) {
          cycle = true;
          break;
        }

        temp_cdg[ch1].insert(ch2);
      }

      if (!cycle) {
        possible_layer = layer;
        if (highest_layer < possible_layer) {
          highest_layer = possible_layer;
        }

        layer_assignment[src][dst] = possible_layer;

        for (size_t i = 0; i < path.size() - 1; ++i) {
          int current = path[i];
          int next = path[i + 1];
          routing_tables[possible_layer][current][dst] = next;
        }

        for (const auto &kv : temp_cdg) {
          int from_ch = kv.first;
          for (int to_ch : kv.second) {
            layer_cdg[possible_layer][from_ch].insert(to_ch);
          }
        }

        assigned = true;
        break;
      }
    }

    if (!assigned && lash_fallback) {
      layer_assignment[src][dst] = -1;
    } else if (!assigned) {
      assert(false && "NOT ENOUGH LAYERS FOR ALL PATHS");
    }
  }

  std::cout << "This topology requires " << (highest_layer + 1) << " layers, "
            << n_layers << " were assigned" << std::endl;
  assert(false);
}

void lash_routing::build_lash_tables() const {
  // Initialize structures
  layer_assignment.assign(n_routers, std::vector<int>(n_routers, -1));

  // Routing tables are [layer][current][destination] = next_hop
  routing_tables.assign(
      n_layers, std::vector<std::vector<int>>(n_routers,
                                              std::vector<int>(n_routers, -1)));

  // Channel dependency graph per layer
  std::vector<std::unordered_map<int, std::unordered_set<int>>> layer_cdg(
      n_layers);

  // Process all source-destination pairs
  std::vector<std::pair<int, int>> src_dst_pairs;
  for (int src = 0; src < n_routers; ++src) {
    for (int dst = 0; dst < n_routers; ++dst) {
      if (src != dst) {
        src_dst_pairs.push_back({src, dst});
      }
    }
  }

  highest_layer = 0;

  for (const auto &[src, dst] : src_dst_pairs) {
    std::vector<int> path = shortest_path(src, dst);
    if (path.empty())
      continue;

    bool assigned = false;
    bool cycle = false;
    int possible_layer = 0;

    int max_regular_layer = lash_fallback ? (n_layers - 1) : n_layers;

    for (int layer = 0; layer < max_regular_layer; ++layer) {
      cycle = false;

      // Temporary CDG edges introduced by this candidate path
      std::unordered_map<int, std::unordered_set<int>> temp_cdg;

      // Only consecutive channels create dependencies:
      // (a->b) -> (b->c)
      for (size_t i = 1; i + 1 < path.size(); ++i) {
        int a = path[i - 1];
        int b = path[i];
        int c = path[i + 1];

        int ch1 = ChanId(a, b, n_routers);
        int ch2 = ChanId(b, c, n_routers);

        if (ReachableChannelWithTemp(layer_cdg[layer], temp_cdg, ch2, ch1)) {
          cycle = true;
          break;
        }

        temp_cdg[ch1].insert(ch2);
      }

      if (!cycle) {
        possible_layer = layer;
        if (highest_layer < possible_layer) {
          highest_layer = possible_layer;
        }

        // Safe to add to this layer
        layer_assignment[src][dst] = possible_layer;

        for (size_t i = 0; i < path.size() - 1; ++i) {
          int current = path[i];
          int next = path[i + 1];
          routing_tables[possible_layer][current][dst] = next;
        }

        for (const auto &kv : temp_cdg) {
          int from_ch = kv.first;
          for (int to_ch : kv.second) {
            layer_cdg[possible_layer][from_ch].insert(to_ch);
          }
        }

        assigned = true;
        break;
      }
    }

    if (!assigned && lash_fallback) {
      layer_assignment[src][dst] = -1;
    } else if (!assigned) {
      assert(false && "NOT ENOUGH LAYERS FOR ALL PATHS");
    }
  }

  std::cout << "REACHED" << std::endl;
}

void lash_routing::compute_prev_router_map(
    const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
        router_list,
    int n_routers) const {

  prev_router_map.clear();
  for (int rid = 0; rid < n_routers; ++rid) {
    for (const auto &e : router_list[1].at(rid)) {
      int next = e.first;
      int in_port = std::get<1>(e.second);
      prev_router_map[next][in_port] = rid;
    }
  }
}

int lash_routing::get_prev_router(int cur, int in_port) const {
  if (in_port < 0 || cur >= (int)prev_router_map.size())
    return -2;

  if (prev_router_map[cur].count(in_port)) {
    return prev_router_map[cur][in_port];
  }

  return -1;
}

std::vector<std::tuple<int, int, int>> lash_routing::route(
    std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
        router_list,
    int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid,
    int in_port, int flit_id) const {

  // Initialize if needed
  if (!initialized) {
    initialize(router_list, n_routers, n_vcs);
  }

  // If at destination router, deliver to node
  if (cur_rid == dst_rid) {
    if (router_list[0].count(cur_rid)) {
      const auto &node_map = router_list[0].at(cur_rid);
      auto it = node_map.find(dst_nid);
      if (it != node_map.end()) {
        int port = std::get<0>(it->second);
        // For ejection, use all VCs
        return {std::make_tuple(port, 0, n_vcs - 1)};
      }
    }
    return {};
  }

  int layer = layer_assignment[src_rid][dst_rid];

  if (layer == -1) {
    // Use escape routing
    std::vector<std::tuple<int, int, int>> result =
        fallback_routing->route(router_list, n_routers, n_nodes, n_vcs, src_rid, cur_rid, cur_vc, dst_rid,
                                dst_nid, in_port, flit_id);
    std::vector<std::tuple<int, int, int>> new_result;
    int fallback_vc_start = ((n_vcs / n_layers) * n_layers) - 1;
    for (std::tuple<int, int, int> tuple : result) {
      int outport = std::get<0>(tuple);
      new_result.push_back(
          std::make_tuple(outport, fallback_vc_start, n_vcs - 1));
    }
    if (new_result.size() == 0) {
      std::cout << "FALLBACK RETURNS EMPTY" << std::endl;
    }
    return new_result;
  }

  int next_hop = -1;
  if (layer < n_layers - 1 || !lash_fallback) {
    if (layer >= (int)routing_tables.size() ||
        cur_rid >= (int)routing_tables[layer].size() ||
        dst_rid >= (int)routing_tables[layer][cur_rid].size()) {
      std::cerr << "Error: Routing table out of bounds" << std::endl;
      return {};
    }
    next_hop = routing_tables[layer][cur_rid][dst_rid];
  }

  if (next_hop == -1) {
    std::cerr << "Error: No next hop found for route from " << cur_rid << " to "
              << dst_rid << " on layer " << layer << std::endl;
    return {};
  }

  std::vector<std::tuple<int, int, int>> result;
  if (edge_ports[cur_rid].count(next_hop)) {
    for (int port : edge_ports[cur_rid].at(next_hop)) {
      int vcs_per_layer = n_vcs / n_layers;
      if (vcs_per_layer < 1)
        vcs_per_layer = 1;

      int vc_begin = layer * vcs_per_layer;
      int vc_end = vc_begin + vcs_per_layer - 1;

      if (vc_begin >= 0 && vc_end < n_vcs && vc_begin <= vc_end) {
        result.emplace_back(port, vc_begin, vc_end);
      }
    }
  }

  if (result.empty()) {
    std::cout << "NORMAL RETURNS EMPTY for route from " << cur_rid << " to "
              << dst_rid << " via next hop " << next_hop << std::endl;
  }

  return result;
}

void lash_routing::print_layer_statistics() const {
  if (!initialized) {
    std::cout << "LASH routing not initialized yet" << std::endl;
    return;
  }

  std::vector<int> layer_counts(n_layers, 0);
  int total_paths = 0;

  // Count paths assigned to each layer (including escape layer)
  for (int src = 0; src < n_routers; ++src) {
    for (int dst = 0; dst < n_routers; ++dst) {
      if (src != dst) {
        total_paths++;
        int layer = layer_assignment[src][dst];
        if (layer == -1) {
          layer_counts[n_layers - 1]++;
        } else if (layer >= 0 && layer < n_layers) {
          layer_counts[layer]++;
        }
      }
    }
  }

  std::cout << "\n=== LASH Routing Layer Statistics ===" << std::endl;
  std::cout << "Total source-destination pairs: " << total_paths << std::endl;
  std::cout << "Number of layers: " << n_layers << " (Layer " << n_layers - 1
            << " is the escape layer)" << std::endl;

  int max_count = 0;
  for (int i = 0; i < n_layers; ++i) {
    if (layer_counts[i] > max_count)
      max_count = layer_counts[i];
  }

  for (int i = 0; i < n_layers; ++i) {
    double percentage =
        (total_paths > 0) ? (100.0 * layer_counts[i] / total_paths) : 0;

    if (i == n_layers - 1) {
      std::cout << "Layer " << i << " (Escape): " << layer_counts[i]
                << " paths (" << std::fixed << percentage << "%)" << std::endl;
    } else {
      std::cout << "Layer " << i << ": " << layer_counts[i] << " paths ("
                << std::fixed << percentage << "%)" << std::endl;
    }

    if (max_count > 0) {
      int bar_length = 50 * layer_counts[i] / max_count;
      std::cout << "  [" << std::string(bar_length, '#')
                << std::string(50 - bar_length, ' ') << "] " << layer_counts[i]
                << std::endl;
    }
  }

  std::cout << "\n=== Summary ===" << std::endl;
  double regular_percentage = 0;
  for (int i = 0; i < n_layers - 1; ++i) {
    regular_percentage += (100.0 * layer_counts[i] / total_paths);
  }
  double escape_percentage =
      (total_paths > 0) ? (100.0 * layer_counts[n_layers - 1] / total_paths)
                        : 0;

  std::cout << "Regular layers (0-" << n_layers - 2
            << "): " << total_paths - layer_counts[n_layers - 1] << " paths ("
            << std::fixed << regular_percentage << "%)" << std::endl;
  std::cout << "Escape layer (Layer " << n_layers - 1
            << "): " << layer_counts[n_layers - 1] << " paths (" << std::fixed
            << escape_percentage << "%)" << std::endl;

  if (n_layers > 1) {
    std::cout << "\n=== Load Balance ===" << std::endl;
    double avg_paths_per_layer = static_cast<double>(total_paths) / n_layers;
    std::cout << "Average paths per layer: " << std::fixed
              << avg_paths_per_layer << std::endl;

    double variance = 0;
    for (int i = 0; i < n_layers; ++i) {
      double diff = layer_counts[i] - avg_paths_per_layer;
      variance += diff * diff;
    }
    variance /= n_layers;
    double stddev = std::sqrt(variance);
    std::cout << "Standard deviation: " << std::fixed << stddev << std::endl;

    int min_layer = 0, max_layer = 0;
    for (int i = 1; i < n_layers; ++i) {
      if (layer_counts[i] < layer_counts[min_layer])
        min_layer = i;
      if (layer_counts[i] > layer_counts[max_layer])
        max_layer = i;
    }

    std::cout << "Most loaded: Layer " << max_layer << " ("
              << layer_counts[max_layer] << " paths)" << std::endl;
    std::cout << "Least loaded: Layer " << min_layer << " ("
              << layer_counts[min_layer] << " paths)" << std::endl;
  }
}
