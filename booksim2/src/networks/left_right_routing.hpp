#ifndef _LEFT_RIGHT_ROUTING_HPP_
#define _LEFT_RIGHT_ROUTING_HPP_

#include "../config_utils.hpp"
#include "routing_function.hpp"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class left_right_routing : public routing_function {
public:
  // Constructor
  left_right_routing(const Configuration &config);

  // Destructor
  virtual ~left_right_routing() = default;

  // Main routing function (implements the pure virtual from routing_function)
  std::vector<std::tuple<int, int, int>>
  route(std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
            router_list,
        int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid,
        int dst_rid, int in_port, int flit_id) const override;


  void increment_hop_budget(int flit_id) override;

  // Threshold mode enumeration
  enum class ThresholdMode {
    SHORTEST, // Only shortest paths
    BOUNDED,  // Paths bounded by tree distance
    LIMITED,  // Paths limited by MAX_HOPS
    ALL       // All legal paths
  };

  // Public utility functions (for debugging)
  void print_topology() const;
  void debug_print_state() const;

private:
  const Configuration &_config;

  // Configuration parameters
  mutable ThresholdMode threshold_mode;
  mutable int MAX_HOPS;

  // Core data structures (mutable for caching)
  mutable bool initialized = false;
  mutable int root;
  mutable std::vector<int> parent;
  mutable std::vector<int> width;                 // Width-first search ordering
  mutable std::vector<int> depth;                 // Tree depth from root
  mutable std::vector<std::vector<int>> adj;      // Adjacency list
  mutable std::vector<std::vector<int>> bfs_tree; // BFS tree structure
  mutable std::map<int, std::map<int, std::vector<int>>>
      edge_ports;                            // Port mappings
  mutable std::map<int, int> node_to_router; // Node ID -> Router ID mapping
  mutable std::vector<std::map<int, int>>
      prev_router_map;                    // (router, in_port) -> prev_router
  mutable std::vector<bool> source_built; // Whether paths computed for source

  // Routing tables
  mutable std::vector<std::map<int, std::vector<int>>>
      left_candidates; // Left turn paths
  mutable std::vector<std::map<int, std::vector<int>>>
      right_candidates; // Right turn paths

  // Hop tracking per flit
  mutable std::map<int, int> fID_HOPS; // flit_id -> remaining hops

  // Precomputed minimum hops for turn-constrained routing
  // Structure: min_hops_to_dst[prev][src][dst] = minimum hops
  mutable std::map<int, std::map<int, std::map<int, int>>> min_hops_to_dst;

  // Port usage statistics (commented out in implementation)
  // mutable std::vector<int> port_possibilities;

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

  // Get previous router based on current router and input port
  int get_prev_router(int cur, int in_port) const;

  // Check if edge is on the tree
  bool is_tree_edge(int from, int to) const;
  // Get tree distance between two routers (via BFS tree)
  int get_tree_distance(int src_rid, int dst_rid) const;

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

  // Get output ports between routers
  std::vector<int> get_ports(int from_rid, int to_rid) const;

  // Check if a turn is legal (no left-right conflicts)
  bool is_legal_turn(int prev_rid, int cur_rid, int next_rid) const;

  // Debug helper
  // void print_possibilities() const;
};

#endif // _LEFT_RIGHT_ROUTING_NEW_HPP_
