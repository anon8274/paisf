#pragma once
#include "routing_function.hpp"
#include <random>
#include <vector>
#include <tuple>
#include <map>

class prefix_routing : public routing_function {
public:
    prefix_routing() = default;
	// router_list[0][router_id][node_id] = (port, latency)
	// router_list[1][router_id][other_router_id] = (output_port_at_src_router, input_port_at_dst_router, latency)
	// return: (out_port, vc_start, vc_end)
    std::vector<std::tuple<int,int,int>> route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const override;
private:
	mutable std::vector<std::string> router_labels;	 					// router_labels[router_id] = label
	mutable std::map<std::pair<int,int>, std::string> link_labels;   	// link_labels[{cur_rid,nxt_rid}] = label
	mutable std::map<int, std::map<int, int>> routing_table; 		 	// routing_table[cur_rid][dst_rid] = out_port

	int get_prefix_length(const std::string& label_1, const std::string& label_2) const;
	void assign_labels(std::vector<std::map<int, std::map<int, std::tuple<int,int,int>>>>& router_list, int n_routers) const;
	void route_from_source_to_destination(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int src_rid, int dst_rid) const;

};


