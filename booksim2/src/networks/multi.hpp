#pragma once

#include <map>
#include <vector>
#include <tuple>

#include "routing_function.hpp"

void multi_compute_distinct_paths_for_one_src_dst_pair(std::map<int, std::map<std::tuple<int,int,int,int>, std::vector<std::tuple<int,int,int>>>>& multi_routing_table, std::map<int,int> node_list, std::vector<std::map<int, std::map<int, std::tuple<int,int,int>>>> router_list, std::map<int, std::map<int, int>> router_and_outport_to_next_router, int n_vcs, int src_rid, int dst_rid, routing_function& rf);


