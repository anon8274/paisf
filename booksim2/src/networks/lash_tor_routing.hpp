
#ifndef LASH_TOR_ROUTING_HPP
#define LASH_TOR_ROUTING_HPP

#include <fstream>
#include <cereal/types/vector.hpp>
#include <cereal/archives/binary.hpp>

#include "left_right_routing.hpp"
#include "lturn_routing.hpp"
#include "routing_function.hpp"
#include "up_down_routing.hpp"

#include <climits>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Configuration;

class lash_tor_routing : public routing_function {
public:
	explicit lash_tor_routing(const Configuration &config);

	std::vector<std::tuple<int, int, int>>
	route(std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const override;

private:
	const Configuration &_config;
	mutable bool initialized = false;

	// total virtual layers including fallback
	mutable int n_layers; // normal layers = n_layers-1
	mutable int num_vcs;
	mutable int normal_layers;	// number of LASH layers
	mutable int fallback_layer; // virtual layer id
	mutable std::string fallback_alg;

	mutable uint32_t shuffle_seed = 1;

	// -------- topology --------
	mutable int topo_n_routers;

	mutable std::unordered_map<int, int> node_to_router; // node_id -> router_id
	mutable std::vector<char> router_has_nodes; // router has attached nodes
	mutable std::vector<int> routers_with_nodes;

	mutable std::vector<std::vector<int>> adj; // neighbors
	mutable std::unordered_map<int, std::unordered_map<int, std::vector<int>>>
			edge_ports; // [u][v] -> out ports
	mutable std::unordered_map<int, std::unordered_map<int, int>>
			prev_router_map; // [router][in_port] -> prev_router

	// -------- routing tables (per source) --------
	mutable std::unordered_map<
			int, std::unordered_map<int, std::unordered_map<int, int>>>
			routing_table; // [src][cur][dst] = next
	mutable std::unordered_map<
			int, std::unordered_map<int, std::unordered_map<int, int>>>
			layer_table; // [src][cur][dst] = layer

	mutable std::unordered_map<int, std::unordered_map<int, bool>>
			fallback_paths; // [src][dst] = true if fallback

	// -------- channel dependency graph (CDG) per layer --------
	mutable std::vector<std::unordered_map<int, std::unordered_set<int>>>
			layer_cdg;

	mutable bool dense_reach_enabled = false;
	mutable size_t num_channel_ids = 0;				// n_routers*n_routers
	mutable std::vector<uint32_t> reach_stamp; // dense visited marks
	mutable uint32_t reach_token = 1;					// increments each query

	inline int chan_id(int u, int v) const { return u * topo_n_routers + v; }

	bool reachable_channel(int layer, int start_ch, int target_ch) const;
	bool would_create_cycle_ch(int layer, int from_ch, int to_ch) const;
	bool reachable_channel_with_temp( int layer, int start_ch, int target_ch, const std::unordered_map<int, std::unordered_set<int>> &temp_edges) const;
	void add_cdg_edge(int layer, int from_ch, int to_ch) const;

	// -------- VC mapping --------
	struct VcRange {
		int begin = 0;
		int end = -1;
	};
	mutable std::vector<VcRange>
			layer_to_vc; // size = normal_layers + 1 (fallback at end)
	mutable int fallback_vc_begin = 0;

	void compute_vc_ranges(int n_vcs) const;

	// -------- fallback routing --------
	mutable std::unique_ptr<routing_function> fallback_routing;

	// -------- initialization / build --------
	void initialize(
			const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
					&router_list,
			int n_routers, int n_vcs) const;

	void build_node_maps(
			const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
					&router_list,
			int n_routers) const;

	void build_links(
			const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
					&router_list,
			int n_routers) const;

	void compute_prev_router_map(
			const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
					&router_list) const;

	// -------- shortest path search --------
	struct DagInfo {
		std::vector<int> dist; // dist from src
		std::vector<std::vector<int>>
				pred; // pred[v] = routers that precede v on shortest paths
	};

	DagInfo build_shortest_path_dag(int src, int n_routers) const;

	struct BestPath {
		bool found = false;
		int layers_used = INT_MAX;
		std::vector<int> path;			 // router sequence src..dst
		std::vector<int> hop_layers; // layer per hop (size path.size()-1)
	};

	BestPath find_best_shortest_path_all(const DagInfo &dag, int src,
																			 int dst) const;

	void dfs_all_shortest_paths(int node, int src, const DagInfo &dag,
															std::vector<int> &rev_path, BestPath &best,
															int dst) const;

	// Assign layers on a full path using channel-based cycle checks.
	// Adds a CDG dependency only when two consecutive hops are in same layer.
	bool assign_layers_for_path(const std::vector<int> &path,
															int &layers_used_out,
															std::vector<int> &hop_layers_out) const;

	void commit_path(int src, int dst, const std::vector<int> &path,
									 const std::vector<int> &hop_layers) const;

	 bool build_one_vc_mode_or_all_fallback(
      const std::vector<std::pair<int, int>> &pairs, int n_routers) const;

	// --------- balance layers -----------
	mutable bool fallback_promoted_for_balance = false;
	mutable int active_balance_layers = 0;

	// Check if fallback is used or can be normal layer
	void maybe_promote_fallback_for_balance() const;
	// Stored committed path per (src,dst), for post-build balancing
	mutable std::unordered_map<int, std::unordered_map<int, BestPath>>
			committed_paths;

	// Refcounted CDG edges so we can safely remove/add during balancing
	// layer_cdg_refcnt[layer][from_ch][to_ch] = count
	mutable std::vector<std::unordered_map<int, std::unordered_map<int, int>>>
			layer_cdg_refcnt;

	// Balancing
	void balance_layer_loads() const;
	std::vector<int> compute_layer_usage() const;

	bool try_move_segment_up(int src, int dst, BestPath &bp, int seg_begin_hop,
													 int seg_end_hop, int target_layer,
													 std::vector<int> &layer_usage) const;

	void add_cdg_edge_ref(int layer, int from_ch, int to_ch) const;
	void remove_cdg_edge_ref(int layer, int from_ch, int to_ch) const;

	// helpers
	std::vector<int> get_ports(int u, int v) const;

	void validate_tables(
			const std::vector<std::map<int, std::map<int, std::tuple<int, int, int>>>>
					&router_list,
			int n_routers) const;
	bool has_cycle_in_cdg_layer(int layer) const;
	void validate_cdg() const;


	// Stuff needed for caching of routing tables
	int repetition;
	string cache_directory;

	struct RoutingEntry {
		int src;
		int cur;
		int dst;
		int value; // next_hop for routing_table, layer for layer_table

		template<class Archive>
		void serialize(Archive &ar) {
			ar(src, cur, dst, value);
		}
	};

	struct FallbackEntry {
		int src;
		int dst;
		bool is_fallback;

		template<class Archive>
		void serialize(Archive &ar) {
			ar(src, dst, is_fallback);
		}
	};

	class LashTorRoutingTable {
	public:
		mutable std::unordered_map<int, std::unordered_map<int, std::unordered_map<int,int>>> routing_table;
		mutable std::unordered_map<int, std::unordered_map<int, std::unordered_map<int,int>>> layer_table;
		mutable std::unordered_map<int, std::unordered_map<int, bool>> fallback_paths;

		// Save to disk
		void save(const std::string &filename) const {
			std::vector<RoutingEntry> flat_routing, flat_layer;
			std::vector<FallbackEntry> flat_fallback;

			// Flatten routing_table
			for (const auto &[src, m_cur] : routing_table)
				for (const auto &[cur, m_dst] : m_cur)
					for (const auto &[dst, next_hop] : m_dst)
						flat_routing.push_back({src, cur, dst, next_hop});

			// Flatten layer_table
			for (const auto &[src, m_cur] : layer_table)
				for (const auto &[cur, m_dst] : m_cur)
					for (const auto &[dst, layer] : m_dst)
						flat_layer.push_back({src, cur, dst, layer});

			// Flatten fallback_paths
			for (const auto &[src, m_dst] : fallback_paths)
				for (const auto &[dst, is_fallback] : m_dst)
					flat_fallback.push_back({src, dst, is_fallback});

			std::ofstream os(filename, std::ios::binary);
			cereal::BinaryOutputArchive archive(os);
			archive(flat_routing, flat_layer, flat_fallback);
		}

		// Load from disk
		void load(const std::string &filename) const {
			std::vector<RoutingEntry> flat_routing, flat_layer;
			std::vector<FallbackEntry> flat_fallback;

			std::ifstream is(filename, std::ios::binary);
			if (!is)
				return; // file doesn't exist

			cereal::BinaryInputArchive archive(is);
			archive(flat_routing, flat_layer, flat_fallback);

			// Clear previous data
			routing_table.clear();
			layer_table.clear();
			fallback_paths.clear();

			// Reconstruct routing_table
			for (const auto &entry : flat_routing)
				routing_table[entry.src][entry.cur][entry.dst] = entry.value;

			// Reconstruct layer_table
			for (const auto &entry : flat_layer)
				layer_table[entry.src][entry.cur][entry.dst] = entry.value;

			// Reconstruct fallback_paths
			for (const auto &entry : flat_fallback)
				fallback_paths[entry.src][entry.dst] = entry.is_fallback;
		}
	}; 

};

#endif
