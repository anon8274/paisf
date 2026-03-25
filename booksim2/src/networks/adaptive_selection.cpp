#include "adaptive_selection.hpp"
#include "flit.hpp"
#include "router.hpp"
#include <vector>
#include <tuple>
#include <random>
#include <iostream>
#include <cassert>

// Selects a next hop based on the input buffer occupancy of the next router.
std::tuple<int,int,int> adaptive_selection::select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const {
    if (valid_next_hops.empty()) {
        std::cout << "Error: No valid next hops available for adaptive selection function." << std::endl;
        assert(false);
    }
	Router* current_router = (*router_object_list_)[rid];
	int min_buffer_occupancy = 1000000; // Initialize to a large number
	std::tuple<int,int,int> selected_hop;
	int output_port, bufer_occupancy;
	for (std::tuple<int,int,int> next_hop : valid_next_hops) {
		output_port = std::get<0>(next_hop);
		for (int vc = std::get<1>(next_hop); vc <= std::get<2>(next_hop); vc++) {
			bufer_occupancy = current_router->GetUsedCreditForVC(output_port, vc);
			if (bufer_occupancy < min_buffer_occupancy) {
				min_buffer_occupancy = bufer_occupancy;
				selected_hop = std::make_tuple(output_port, vc, vc);
			}
		}
	}
    return selected_hop;
}
