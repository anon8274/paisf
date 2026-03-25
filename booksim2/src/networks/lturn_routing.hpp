#ifndef LTURN_ROUTING_HPP
#define LTURN_ROUTING_HPP


#include "routing_function.hpp"
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class Configuration;

class lturn_routing : public routing_function {
public:
  explicit lturn_routing(const Configuration &config);

  std::vector<std::tuple<int, int, int>>
  route(std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
            router_list,
        int n_routers, int n_nodes, int n_vcs, int src_rid, int cur, int cur_vc, int dst_node,
        int dst_rid, int in_port, int flit_id) const override;


  void increment_hop_budget(int flit_id) override;

  void initialize(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          &router_list,
      int n_routers) const;

  // Debugging Utilities
  void print_mst_topology() const;
  void debug_print_state() const;
  void print_hop_list(int n_routers) const;

private:
  // Type Definitions
  enum class ChannelType {
    LU, // Left-Up
    LD, // Left-Down
    RU, // Right-Up
    RD  // Right-Down
  };

  enum class ThresholdMode {
    SHORTEST, // Use only shortest paths
    BOUNDED,  // Use paths within tree distance bound
    LIMITED,  // Use paths within absolute hop limit
    ALL       // Use all legal paths
  };


  // NOTE: Added by PI
  // Format: min_hops_to_dst[prev][cur][dst] = minimum hops from cur to dst when coming from prev
  // pref = -1 means no restrictions on the direction we came from (used at injection)
  mutable std::map<int,std::map<int,std::map<int,int>>> min_hops_to_dst;
  void compute_min_hops_to_dst(const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>> router_list, int n_routers) const;

  // Routing Configuration
  ThresholdMode threshold_mode;
  int MAX_HOPS; // Only used in LIMITED mode
  const Configuration &_config;
  mutable bool initialized = false;

  // Graph Topology
  mutable std::vector<std::vector<int>> adj; // Adjacency lists
  mutable std::vector<std::map<int, std::vector<int>>>
      edge_ports; // Ports per edge
  mutable std::vector<std::map<int, int>>
      port_to_neighbor;                                // Port to neighbor map
  mutable std::unordered_map<int, int> node_to_router; // Node ID to router ID

  // BFS Tree Properties
  mutable std::vector<int> width;       // Left/Right ordering
  mutable std::vector<int> level;       // Up/Down ordering
  mutable std::vector<int> tree_parent; // Parent in BFS tree
  mutable int root;                     // Root router ID

  // Turn Restrictions
  mutable std::vector<std::vector<bool>> prohibited_LD_to_RU;
  mutable std::vector<std::vector<bool>> prohibited_LD_to_RD;

  // Routing State
  mutable std::map<int, int> fID_HOPS; // Remaining hops for flit
  mutable std::map<int, int> fID_SRC;  // Source router for flit

  // Precomputed Routing Tables
  mutable std::vector<
      std::vector<std::vector<std::vector<std::tuple<int, int>>>>>
      hop_list; // [src][cur][dst] -> list of (port, hops_to_dst)

  // For backward path construction during initialization
  mutable std::vector<std::vector<std::vector<std::vector<int>>>>
      add_path; // [src][cur][dst] -> list of parents

  // Previous Router Lookup (for incoming channel type)
  mutable std::map<int, std::map<int, int>> prev_router_map;

  // Private Methods

  // Threshold mode parsing
  ThresholdMode parse_threshold_mode(const std::string &mode_str) const;

  // Channel classification based on level and width
  ChannelType classify_channel(int from, int to) const;
  std::string channelTypeToString(ChannelType ct) const;

  // Outgoing channel enumeration
  std::vector<std::pair<int, ChannelType>>
  get_outgoing_channels(int node) const;

  // Port utilities
  std::vector<int> get_ports_to_neighbor(int cur, int neighbor) const;

  // Tree operations
  bool is_tree_edge(int from, int to) const;
  int compute_lr_tree_distance(int src, int dst) const;
  void compute_prev_router_map(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          &router_list,
      int n_routers) const;
  int get_prev_router(
      int cur, int in_port,
      const std::map<int, std::map<int, std::tuple<int, int, int>>>
          &connections) const;

  // Turn legality checking
  bool is_turn_legal(ChannelType in_ct, ChannelType out_ct, int router,
                     int port) const;

  // Cycle detection for prohibited turns (paper algorithm)
  bool is_turn_already_prohibited(int from, int to, ChannelType in_ct,
                                  ChannelType out_ct) const;
  bool is_returning_via_ld(int current, int next, int start_node) const;
  bool find_cycles_from_node(
      int start_node, bool is_case_a,
      std::vector<std::pair<int, int>> &prohibited_turns) const;
  std::vector<int> select_nodes_by_criteria() const;
  void detect_prohibited_turns(int n_routers) const;

  // Path computation
  std::vector<int> compute_next_hops(const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>> router_list, int src_router, int cur_router, int dst_router, int prev, int flit_id, ChannelType in_ct) const;

  // Routing table computation
  void compute_hop_list(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          router_list,
      int n_routers, ChannelType in_ct) const;
};

#endif // LTURN_ROUTING_HPP
