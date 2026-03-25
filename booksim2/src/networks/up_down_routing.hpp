#ifndef _UP_DOWN_ROUTING_HPP_
#define _UP_DOWN_ROUTING_HPP_

#include "../config_utils.hpp"
#include "routing_function.hpp"
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class up_down_routing : public routing_function {
public:
  // Constructor
  up_down_routing(const Configuration &config);

  void set_fID_HOPS(int flit_id, int hops) const;
  // Destructor
  virtual ~up_down_routing() = default;

  // Main routing function (implements the pure virtual from routing_function)
  std::vector<std::tuple<int, int, int>> route(std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const override;


  void increment_hop_budget(int flit_id) override;

  // Threshold mode enumeration
  enum class ThresholdMode {
    SHORTEST, // Only shortest paths
    BOUNDED,  // Paths bounded by tree distance
    LIMITED,  // Paths limited by MAX_HOPS
    ALL       // All legal paths
  };

private:
  const Configuration &_config;
  mutable std::vector<int> port_possibilities;

  // Configuration parameters
  mutable ThresholdMode threshold_mode;
  mutable int MAX_HOPS;

  // Core data structures (mutable for caching)
  mutable bool initialized = false;
  mutable int root;
  mutable std::vector<int> parent;
  mutable std::vector<int> level; // BFS tree level for up/down classification
  mutable std::vector<std::vector<int>> adj; // Adjacency list
  mutable std::map<int, int> node_to_router; // Node ID -> Router ID mapping
  mutable std::map<int, std::map<int, int>>
      prev_router_map; // (router, in_port) -> prev_router

  // Hop tracking per flit
  mutable std::map<int, int> fID_HOPS; // flit_id -> remaining hops

  // Precomputed minimum hops for turn-constrained routing
  // Structure: min_hops_to_dst[prev][src][dst] = minimum hops
  mutable std::map<int, std::map<int, std::map<int, int>>> min_hops_to_dst;

  // Parse threshold mode from config string
  ThresholdMode parse_threshold_mode(const std::string &mode_str) const;

  // Initialization functions
  void initialize(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          router_list,
      int n_routers) const;

  // Build previous router map for reverse path tracking
  void compute_prev_router_map(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          router_list,
      int n_routers) const;

  // Get previous router by checking in_port of current router
  int get_prev_router(int cur, int in_port) const;
  // Check if an edge is part of the BFS tree
  bool is_tree_edge(int from, int to) const;

  // Precompute minimum hops for all (prev, src, dst) combinations
  void compute_min_hops_to_dst(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          router_list,
      int n_routers) const;

  // Compute valid next hops based on threshold mode and remaining hops
  std::vector<int> compute_next_hops(
      std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          router_list,
      int cur_rid, int dst_rid, int prev_rid, int flit_id) const;

  // Get distance along tree edges from src to dst
  int get_tree_distance(int src, int dst) const;
};

#endif // _UP_DOWN_ROUTING_V6_HPP_
