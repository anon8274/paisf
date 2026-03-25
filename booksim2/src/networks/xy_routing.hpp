#pragma once
#include "routing_function.hpp"
#include <vector>
#include <tuple>
#include <map>

class xy_routing : public routing_function {
public:
    xy_routing() = default;
	// router_list[0][router_id][node_id] = (port, latency)
	// router_list[1][router_id][other_router_id] = (port, latency)
	// return: (out_port, vc_start, vc_end)
    std::vector<std::tuple<int,int,int>> route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const override;
};
