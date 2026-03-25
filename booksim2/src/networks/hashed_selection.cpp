#include "hashed_selection.hpp"
#include "hash_function.hpp"
#include "flit.hpp"
#include <vector>
#include <tuple>
#include <iostream>
#include <cassert>
#include <cstdint>

// Selects a next hop based on hashing a packet's entropy value.
std::tuple<int,int,int> hashed_selection::select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const {
	if (valid_next_hops.empty()) {
		std::cout << "Error: No valid next hops available for hashed selection function." << std::endl;
		assert(false);
	}
	// We need to make sure to always select the same output port and VC for all subsequent packets of the same message
	// For this, we make a flat list with next hops, one per VC
	// This assumes that the routing function always returns the same next hops (same ports, same VC ranges, same order) for identical cur and dst routers.
	std::vector<std::tuple<int,int,int>> flat_next_hop_list;
	for (std::tuple<int,int,int> next_hop : valid_next_hops) {
		for (int vc = std::get<1>(next_hop); vc <= std::get<2>(next_hop); vc++) {
			flat_next_hop_list.push_back(std::make_tuple(std::get<0>(next_hop), vc, vc));
		}
	}
	std::tuple<int,int,int> selected_hop = flat_next_hop_list[hash_function(f->entropy, rid, flat_next_hop_list.size())];
	return selected_hop;
}

