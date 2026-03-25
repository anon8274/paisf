#include "deterministic_selection.hpp"
#include "flit.hpp"
#include <vector>
#include <tuple>
#include <iostream>
#include <cassert>

// Forward declaration of Flit to avoid circular dependency
class Flit; 

// Selects a deterministic next hop from the list of valid next hops
std::tuple<int,int,int> deterministic_selection::select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const {
	if (valid_next_hops.empty()) {
		std::cout << "Error: No valid next hops available for deterministic selection function." << std::endl;
		assert(false);
	}
	int vc = get<1>(valid_next_hops[0]);
	tuple<int,int,int> selected_next_hop = valid_next_hops[0];
	get<1>(selected_next_hop) = vc;
	get<2>(selected_next_hop) = vc;
	return selected_next_hop;
}

