#ifndef LASH_ROUTING_HPP
#define LASH_ROUTING_HPP

#include "left_right_routing.hpp"
#include "lturn_routing.hpp"
#include "routing_function.hpp"
#include "up_down_routing.hpp"
#include <climits>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_set>
#include <vector>

class Configuration; // Forward declaration

class lash_routing : public routing_function {
public:
  lash_routing(const Configuration &config);

  void initialize(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          &router_list,
      int n_routers, int n_vcs) const;

  std::vector<std::tuple<int, int, int>>
  route(std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
            router_list,
        int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid,
        int in_port, int flit_id) const;

  void print_layer_statistics() const;
  void print_path_details() const;

private:
  const Configuration &_config;
  mutable bool lash_fallback = false;
  mutable int n_layers;
  mutable int n_routers;
  mutable bool initialized = false;
  mutable string fallback_alg;
  mutable int highest_layer;
  mutable bool debug_mode = false;

  // Routing tables
  mutable std::vector<std::vector<std::vector<int>>>
      routing_tables; // [layer][src][dst] = next_hop
  mutable std::vector<std::vector<int>> layer_assignment; // [src][dst] = layer
  mutable std::vector<std::vector<int>> escape_routing; // [src][dst] = next_hop

  // Topology information
  mutable std::map<int, int> node_to_router;
  mutable std::vector<std::vector<int>> adj;
  mutable std::vector<std::map<int, std::vector<int>>> edge_ports;

  // Fallback routing
  mutable std::unique_ptr<class routing_function> fallback_routing;

  // Private helper methods
  std::vector<int> shortest_path(int src, int dst) const;
  bool creates_cycle(const std::vector<std::set<int>> &deps, int from,
                     int to) const;
  inline int chan_id(int u, int v) const;

  bool
  reachable_channel(const std::unordered_map<int, std::unordered_set<int>> &cdg,
                    int start_ch, int target_ch) const;

  bool reachable_channel_with_temp(
      const std::unordered_map<int, std::unordered_set<int>> &real_cdg,
      const std::unordered_map<int, std::unordered_set<int>> &temp_cdg,
      int start_ch, int target_ch) const;
  void build_lash_tables() const;
  void build_lash_tables_debug() const;
  void compute_prev_router_map(
      const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
          router_list,
      int n_routers) const;
  int get_prev_router(int cur, int in_port) const;
  mutable std::map<int, std::map<int, int>>
      prev_router_map;                // (router, in_port) -> prev_router
};

#endif // LASH_ROUTING_HPP
