#include "up_down_routing.hpp"
#include "../config_utils.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <iostream>
#include <map>
#include <queue>
#include <tuple>
#include <vector>

up_down_routing::up_down_routing(const Configuration &config)
    : _config(config) {

  // Parse threshold mode from config
  std::string mode_str = _config.GetStr("threshold_mode");
  // set private variable threshold mode to the threshold_mode in the config
  // file
  threshold_mode = parse_threshold_mode(mode_str);

  // If threshold_mode is limited, user eneds to set limit
  // which will be stored in MAX_HOPS
  if (mode_str == "limited" || mode_str == "2") {
    // store user set threshold_hops in MAX_HOPS
    MAX_HOPS = _config.GetInt("threshold_hops");
  } else {
    // if any other mode is used, MAX_HOPS is not required. Set to -1
    MAX_HOPS = -1;
  }

  // Check that a valid MAX_HOPS has been set for LIMITED mode
  if (threshold_mode == ThresholdMode::LIMITED && MAX_HOPS <= 0) {
    std::cerr << "Error: max_hops must be positive when using 'limit' mode\n";
    exit(-1);
  }
}

// Parse threshold mode using the threshold_mode string from the config
up_down_routing::ThresholdMode
up_down_routing::parse_threshold_mode(const std::string &mode_str) const {

  // For all 4 differet modes, set return ThresholdMode::MODE
  if (mode_str == "shortest" || mode_str == "0") {
    return ThresholdMode::SHORTEST;
  } else if (mode_str == "bounded" || mode_str == "1") {
    return ThresholdMode::BOUNDED;
  } else if (mode_str == "limited" || mode_str == "2") {
    return ThresholdMode::LIMITED;
  } else if (mode_str == "all" || mode_str == "3") {
    return ThresholdMode::ALL;
  } else {
    // If no threshold_mode is set or a typo was made, default to SHORTEST
    std::cerr << "Warning: Unknown up_down_threshold_mode '" << mode_str
              << "', defaulting to 'shortest'\n";
    return ThresholdMode::SHORTEST;
  }
}

// Initialization called once when route is first called
void up_down_routing::initialize(
    const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
        router_list,
    int n_routers) const {

  // if u is a parent of v in BFS Tree then parent[v] = u
  parent.assign(n_routers, -1);
  // level in the bfs tree, level[root] = 0, level[leaf] = log n
  level.assign(n_routers, -1);
  // contains a mapping from a node to its connected router
  node_to_router.clear();
  // if u and v have a direct edge then adj[u].contains(v) and
  // adj[v].contains(u)
  adj.assign(n_routers, {});
  // For debugging how many next hops are given to the selection function
  // port_possibilities.assign(10, 0);

  // get random root
  root = rand() % n_routers;

  // create a mapping from dst_nid to dst_rid
  // For every router
  for (int rid = 0; rid < n_routers; ++rid)
    // For this router go through every connected node
    for (const auto &n : router_list[0].at(rid))
      // Add for every node this router as its connected router
      node_to_router[n.first] = rid;

  // compute adjacency
  // u and v are adjacent if u -> v is a direct edge
  // this will go thorugh all routers, therefore v -> u will be added later
  for (int u = 0; u < n_routers; ++u) {
    for (const auto &e : router_list[1].at(u)) {
      int v = e.first;
      adj[u].push_back(v);
    }
  }

  // build bfs trees to determine levels of all routers for up/down comparison
  // queue for bfs
  std::queue<int> q;
  // root is it's own parent
  parent[root] = root;
  // root is level 0, the farther from root, the higher the level
  level[root] = 0;
  q.push(root);

  while (!q.empty()) {
    // cur = next element in the queue
    int cur = q.front();
    q.pop();
    // for every adj node of current
    for (int nei : adj[cur]) {
      // since level is initialized to -1 this tracks visisted,
      // if level != -1 do nothing since we have already been to that node
      if (level[nei] == -1) {
        // set cur as the parent of nei
        parent[nei] = cur;
        // since its bfs the level of the neighbour is 1 bigger than that of its
        // parent
        level[nei] = level[cur] + 1;
        // push neighbor for further processing
        q.push(nei);
      }
    }
  }

  // compute_prev_router_map to lookup which router a package came from
  compute_prev_router_map(router_list, n_routers);
  // compute min hops needed from any router to any dst for any of the
  // prev_router
  compute_min_hops_to_dst(router_list, n_routers);
  // set initialized to true so it doesn't run twice
  initialized = true;
}

// Prev-router map
void up_down_routing::compute_prev_router_map(
    const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
        router_list,
    int n_routers) const {

  // clears router map
  prev_router_map.clear();
  // loop over all routers
  for (int rid = 0; rid < n_routers; ++rid) {
    // loop over all neighbours of this router
    for (const auto &e : router_list[1].at(rid)) {
      // get the neighbours rid
      int next = e.first;
      // get the neighbours in_port when receiving from this router
      int in_port = std::get<1>(e.second);
      // set neighbours prev_router_map on this in_port to this router
      prev_router_map[next][in_port] = rid;
    }
  }
}

int up_down_routing::get_prev_router(int cur, int in_port) const {
  // if the inputs are invalid return -2
  if (in_port < 0 || cur >= (int)prev_router_map.size())
    return -2;

  // if there is a entry for prev_router_map[cur][in_port]
  if (prev_router_map[cur].count(in_port)) {
    // return said value
    return prev_router_map[cur][in_port];
  }

  // if no such entry exist return -1 to show that the package has just been
  // injected
  return -1;
}

// return whether the edge from one router to the other is a edge of the BFS
// tree
bool up_down_routing::is_tree_edge(int from, int to) const {
  // if either node is the others parent it is a tree edge
  // since parent was set in the bfs tree construction in initialize
  return parent[to] == from || parent[from] == to;
}

// computes the distance from src to dst using only tree edges using BFS
int up_down_routing::get_tree_distance(int src, int dst) const {
  // get the number of routers from the size of the adjacency list
  int n_routers = adj.size();
  // create a distance vector initialized to max distance
  vector<int> tree_dist(n_routers, INT_MAX);

  // queue used for BFS
  queue<int> q;
  // set the distance of starting node to 0
  tree_dist[src] = 0;
  q.push(src);

  while (!q.empty()) {
    // u is the current node being processed
    int u = q.front();
    q.pop();

    // for every neighboring node of u
    for (int v : adj[u]) {
      // if the edge from u to v is part of the BFS tree
      if (is_tree_edge(u, v)) {
        // if v is our destination
        if (v == dst)
          // return tree_dist[v] which has not been set but is equal to
          // tree_dist[u] + 1
          return tree_dist[u] + 1;
        // if it is any other node than destination and we have not processed it
        // yet (distance is still INT_MAX)
        if (tree_dist[v] == INT_MAX) {
          // set tree_dist for the neighbor equal to tree_dist[current] + 1 for
          // the one hop from current to nei
          tree_dist[v] = tree_dist[u] + 1;
          // push the neighbor to the end of the queue
          q.push(v);
        }
      }
    }
  }
  // if the destination is never reached, return -1
  return -1;
}

void up_down_routing::compute_min_hops_to_dst(
    const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
        router_list,
    int n_routers) const {
  // Iterate over all source
  for (int src = 0; src < n_routers; src++) {
    // Identify all neighbors of cur
    vector<int> neighbors;
    // Iterate through everything in router_list[1][cur] without using auto
    for (map<int, tuple<int, int, int>>::const_iterator it =
             router_list.at(1).at(src).begin();
         it != router_list.at(1).at(src).end(); ++it) {
      neighbors.push_back(it->first);
    }
    neighbors.push_back(
        -1); // Add -1 to represent the case where we are at the source and have
             // not come from any previous router
    // Iterate through potential previous routers (neighbors of src)
    for (int global_prev : neighbors) {
      // Run Dijkstra from src to all destinations (assuming that we came from
      // global_prev)
      vector<int> dist(n_routers, INT_MAX);
      // We track if edges are visited to allow for visiting the same vertex
      // multiple times (from different directions) while still avoiding
      // checking the same path multiple times.
      vector<pair<int, int>> visited;
      // Our priority queue stores tiples of the form (dist, prev, cur)
      priority_queue<tuple<int, int, int>, vector<tuple<int, int, int>>,
                     greater<tuple<int, int, int>>>
          pq;
      dist[src] = 0;
      pq.push({0, global_prev, src});
      while (!pq.empty()) {
        auto [cur_dist, prev, cur] = pq.top();
        pq.pop();
        // If we already explore coming from prev to cur, skip
        // (the same (prev,cur) pair can be in the queue with multiple dist
        // values, the smallest one will be processed first)
        if (find(visited.begin(), visited.end(), make_pair(prev, cur)) !=
            visited.end()) {
          continue;
        }
        // Mark edge (prev, cur) as visited
        visited.push_back(make_pair(prev, cur));
        // Identify all neighbors of cur, exclude prev as we do not allow 180
        // degree turns
        vector<pair<int, int>> cur_neighbors_with_ports;
        for (map<int, tuple<int, int, int>>::const_iterator it =
                 router_list.at(1).at(cur).begin();
             it != router_list.at(1).at(cur).end(); ++it) {
          if (it->first != prev) {
            cur_neighbors_with_ports.push_back(
                make_pair(it->first, get<0>(it->second)));
          }
        }
        // Iterate through neighbors
        for (pair<int, int> nei : cur_neighbors_with_ports) {
          // Avoid 180 degree turns by skipping the neighbor if it is the same
          // as prev
          if (nei.first == prev) {
            continue;
          }
          // Do not us the same edge twice
          if (find(visited.begin(), visited.end(), make_pair(cur, nei.first)) !=
              visited.end()) {
            continue;
          }
          bool is_legal = true;
          // Check if going from prev over cur to nei is a legal turn
          if (prev != -1) {
            if ((level[prev] < level[cur] ||
                 (level[prev] == level[cur] && prev > cur)) &&
                (level[cur] > level[nei.first] ||
                 (level[cur] == level[nei.first] && cur < nei.first)))
              is_legal = false;
          }
          if (is_legal) {
            // Add this vertex to the queue (also if we did not find a shorter
            // path, we might find a shorter path to a downstream vertex that is
            // not reachable over the shortest path to nei due to turn
            // restrictions)
            pq.push({cur_dist + 1, cur, nei.first});
            // Update distance to nei if we found a shorter path
            if (cur_dist + 1 < dist[nei.first]) {
              dist[nei.first] = cur_dist + 1;
            }
          }
        }
      }
      // Once Dijkstra is done, we know the shortest distance from src to all
      // destinations when coming from global_prev
      for (int dst = 0; dst < n_routers; dst++) {
        min_hops_to_dst[global_prev][src][dst] = dist[dst];
      }
    }
  }
}
vector<int> up_down_routing::compute_next_hops(
    vector<map<int, map<int, tuple<int, int, int>>>> router_list, int cur_rid,
    int dst_rid, int prev_rid, int flit_id) const {
  vector<int> valid_next_hops;

  // Find remaining hops for this flit
  auto it = fID_HOPS.find(flit_id);
  if (it == fID_HOPS.end()) {
    cerr << "ERROR: flit_id " << flit_id << " not found in fID_HOPS" << endl;
    return valid_next_hops; // Return empty vector
  }
  int remaining_hops = it->second;

  // Identify all neighbors of cur, exclude prev as we do not allow 180 degree
  // turns
  vector<pair<int, int>> neighbors_with_ports;
  for (map<int, tuple<int, int, int>>::const_iterator it =
           router_list.at(1).at(cur_rid).begin();
       it != router_list.at(1).at(cur_rid).end(); ++it) {
    // Avoid 180 degree turns by skipping the neighbor if it is the same as
    // prev_rid
    if (it->first != prev_rid) {
      // Only consider neighbors from which the destination is reachable (given
      // that we arrived from cur_rid)
      if (min_hops_to_dst[cur_rid][it->first][dst_rid] < INT_MAX) {
        // Check if going from prev_rid over cur_rid to it->first is a legal
        // turn
        bool is_legal = true;
        if (prev_rid != -1) {
          if ((level[prev_rid] < level[cur_rid] ||
               (level[prev_rid] == level[cur_rid] && prev_rid > cur_rid)) &&
              (level[cur_rid] > level[it->first] ||
               (level[cur_rid] == level[it->first] && cur_rid < it->first))) {
            is_legal = false;
          }
        }
        if (is_legal) {
          neighbors_with_ports.push_back(
              make_pair(it->first, get<0>(it->second)));
        }
      }
    }
  }

  // For shortest paths store shortest available path length
  int shortest_path = INT_MAX;

  // Iterate over valid next hops, check for hop budget according to the
  // threshold mode
  for (pair<int, int> entry : neighbors_with_ports) {
    int nei_rid = entry.first;
    int out_port = entry.second;
    int hops_to_dst_via_port =
        min_hops_to_dst[cur_rid][nei_rid][dst_rid] +
        1; // +1 because we need to add the hop from cur_rid to nei_rid

    if (threshold_mode == ThresholdMode::SHORTEST) {
      // A new shortest path is found, remove all previously stored hops as they
      // were on longer paths
      if (hops_to_dst_via_port < shortest_path) {
        shortest_path = hops_to_dst_via_port;
        valid_next_hops.clear();
      }
      // Add the next hop if it is on a shortest path (supports multiple
      // shortest paths)
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

void up_down_routing::set_fID_HOPS(int flit_id, int hops) const {
  fID_HOPS.insert({flit_id, hops});
}

// Routing function
vector<tuple<int, int, int>> up_down_routing::route(vector<map<int, map<int, tuple<int, int, int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const {

  // if it has never been initialized call initialize
  if (!initialized) {
    initialize(router_list, n_routers);
  }

  // get dst_router from dst_node
  int dst_router = node_to_router.at(dst_nid);

  // If already at destination router, route to the node
  if (cur_rid == dst_router) {
    // go through all nodes at current router
    for (const auto &node_entry : router_list[0].at(cur_rid)) {
      int nid = node_entry.first;
      // if the one of the connected nodes is the dst_node
      if (nid == dst_nid) {
        // get the port from current to node
        int port = std::get<0>(node_entry.second);
        // remove the Remaining Hop entry for the flit that is about to be
        // ejected (this avoids getting a huge table with dead entries)
        fID_HOPS.erase(flit_id);
        // return the output port to the node to the selection function
        return {make_tuple(port, 0, n_vcs - 1)};
      }
    }
  }

  // prev router notes the router that we came from to reach current (-1 at
  // injection)
  int prev_router = get_prev_router(cur_rid, in_port);
  // if flit had just been injected
  if (prev_router == -1) {
    // for the CDG Checker flit ID are negative (bound is unlimited)
    if (flit_id < 0) {
      fID_HOPS.insert({flit_id, n_routers + 1});
    } else {
      // for any of the bounds, set the remaining hops
      // in bounded it's the tree_distance for LIMITED it's MAX_HOPS
      // set to INT_MAX/2 for ALL to allow for basically all paths
      // set to INT_MAX for SHORTEST since shortest is treated in
      // compute_next_hops
      if (threshold_mode == ThresholdMode::BOUNDED) {
        int bound = get_tree_distance(cur_rid, dst_router);
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
  }

  // Get all valid first hops using threshold-aware path computation
  vector<int> first_hops =
      compute_next_hops(router_list, cur_rid, dst_router, prev_router, flit_id);

  // Convert to result format: (port, vc_start, vc_end)
  vector<tuple<int, int, int>> result;
  for (int port : first_hops) {
    result.emplace_back(port, 0, n_vcs - 1);
  }
  // flit is of normal experiment not from CDG Checker decrement the
  // remaining_hops by 1
  if (flit_id >= 0) {
    fID_HOPS[flit_id] -= 1;
  }
  // For printing how many possible ports have been returned to the selection
  // function port_possibilities[result.size()] =
  // port_possibilities[result.size()] + 1; cout << "Possiblities: "; for (int i
  // = 0; i < 10; i++) {
  //   cout << i << ": " << port_possibilities[i] << endl;
  // }
  return result;
}


void up_down_routing::increment_hop_budget(int flit_id) {
	if (fID_HOPS.count(flit_id)) {
	fID_HOPS[flit_id] += 1;
	}
}
