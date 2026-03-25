#include "random_selection.hpp"
#include "flit.hpp"
#include <vector>
#include <tuple>
#include <random>
#include <iostream>
#include <cassert>

// Selects a random next hop from the list of valid next hops
std::tuple<int,int,int> random_selection::select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const {
	if (valid_next_hops.empty()) {
		std::cout << "Error: No valid next hops available for random selection function." << std::endl;
		assert(false);
	}
	std::vector<std::tuple<int,int,int>> flat_next_hop_list;
	for (std::tuple<int,int,int> next_hop : valid_next_hops) {
		for (int vc = std::get<1>(next_hop); vc <= std::get<2>(next_hop); vc++) {
			flat_next_hop_list.push_back(std::make_tuple(std::get<0>(next_hop), vc, vc));
		}
	}
	return flat_next_hop_list[rand() % flat_next_hop_list.size()];
}

