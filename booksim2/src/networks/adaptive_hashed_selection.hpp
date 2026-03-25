#pragma once
#include "selection_function.hpp"
#include <vector>
#include <map>
#include <tuple>

// Forward declaration of Router and Flit to avoid circular dependency
class Router; 
class Flit; 


class adaptive_hashed_selection : public selection_function {
public:
    explicit adaptive_hashed_selection(std::vector<Router*>* router_object_list) : router_object_list_(router_object_list) {}
    std::tuple<int,int,int> select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const override;
private:
    std::vector<Router*>* router_object_list_; 
	mutable std::map<int, std::pair<int,int>> mid_to_outport_and_vc_map_;
};
