#pragma once
#include "routing_function.hpp"
#include <vector>
#include <tuple>
#include <map>

class dfsssp_routing : public routing_function {
public:
    dfsssp_routing() = default;
	// router_list[0][router_id][node_id] = (port, latency)
	// router_list[1][router_id][other_router_id] = (port, latency)
	// return: (out_port, vc_start, vc_end)
    std::vector<std::tuple<int,int,int>> route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const override;
private:
	mutable bool _is_initialized = false;
	mutable std::vector<int> _routers_with_nodes; 												// list of routers that are connected to nodes
    mutable std::map<int,std::map<int,std::map<int, std::tuple<int,int,int>>>> _routing_table; 	// routing_table[cur_rid][src_rid][dst_rid] = (out_port, out_vc, next_rid)

	void initialize(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_vcs) const;
    void sssp_routing(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers) const;
	void search_and_remove_deadlocks(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_vcs) const;
};
