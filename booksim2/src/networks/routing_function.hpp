#pragma once
#include <vector>
#include <tuple>
#include <map>

class routing_function {
public:
    virtual ~routing_function() = default;
    virtual std::vector<std::tuple<int,int,int>> route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vcs, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const = 0;
    virtual void increment_hop_budget(int flit_id) {} // Default implementation does nothing, can be overridden by derived classes if needed
};

