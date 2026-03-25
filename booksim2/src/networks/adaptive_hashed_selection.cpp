#include "adaptive_hashed_selection.hpp"
#include "hash_function.hpp"
#include "flit.hpp"
#include "router.hpp"
#include <vector>
#include <tuple>
#include <random>
#include <iostream>
#include <cassert>
#include <algorithm>

// Selects a next hop based on hashing the entropy value and for the first hop also based on buffer occupancy
std::tuple<int,int,int> adaptive_hashed_selection::select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const {
    if (valid_next_hops.empty()) {
        std::cout << "Error: No valid next hops available for adaptive selection function." << std::endl;
        assert(false);
    }
	std::tuple<int,int,int> selected_hop;
	// If this is the first hop, use buffer-occupancy-based adaptive routing (adaptive)
	if (f->is_first_hop){
		// If this message requires in-order delivery, make sure to use the same output port for all packets of the message
		if (f->requires_in_order_delivery && (mid_to_outport_and_vc_map_.find(f->mid) != mid_to_outport_and_vc_map_.end())) {
			std::pair<int,int> stored_outport_and_vc = mid_to_outport_and_vc_map_[f->mid];
			bool found = false;
			// Find the corresponding next hop tuple
			for (std::tuple<int,int,int> next_hop : valid_next_hops) {
				if ((std::get<0>(next_hop) == stored_outport_and_vc.first) && (std::get<1>(next_hop) <= stored_outport_and_vc.second) && (stored_outport_and_vc.second <= std::get<2>(next_hop))) {
					selected_hop = next_hop;
					found = true;
				}
			}
			if (!found) {
				std::cout << "Error (RID = " << rid << "): Stored output port and VC for in-order message not found in valid next hops." << std::endl;
				assert(false);
			}
			// Overwrite the VC range returned by the routing function with the stored VC since all in-order packets must use the same VC, otherwise they can overtake each other
			// The check and assertion above ensure that the stored VC is within the valid range
			get<1>(selected_hop) = stored_outport_and_vc.second;
			get<2>(selected_hop) = stored_outport_and_vc.second;
			// If this was the last packet of a message, remove the entry from the map
			if (f->is_last) {
				mid_to_outport_and_vc_map_.erase(f->mid);
			}
		// If this message does not require in-order delivery, or if this is the first packet of a message that requires in-order delivery
		} else {
			// Identify the next hop with the minimum buffer occupancy
			Router* current_router = (*router_object_list_)[rid];
			int min_buffer_occupancy = 1000000; // Initialize to a large number
			int output_port, occupancy;
			for (std::tuple<int,int,int> next_hop : valid_next_hops) {
				output_port = std::get<0>(next_hop);
				for (int vc = std::get<1>(next_hop); vc <= std::get<2>(next_hop); vc++) {
					occupancy = current_router->GetUsedCreditForVC(output_port, vc);
					if (occupancy < min_buffer_occupancy) {
						min_buffer_occupancy = occupancy;
						selected_hop = std::make_tuple(output_port, vc, vc);
					}
				}
			}
			// If this is not the last packet of the message (i.e. if this message has more than one packet) 
			// and requires in-order delivery, store the selected output port for future packets of the same message
			if (!f->is_last and f->requires_in_order_delivery) {
				mid_to_outport_and_vc_map_[f->mid] = std::make_pair(std::get<0>(selected_hop), std::get<1>(selected_hop));
				storage_overhead = std::max(storage_overhead, static_cast<int>(mid_to_outport_and_vc_map_.size()));
			}
		}
	} else {
		// We need to make sure to always select the same output port and VC for all subsequent packets of the same message
		// For this, we make a flat list with next hops, one per VC
		// This assumes that the routing function always returns the same next hops (same ports, same VC ranges, same order) for identical cur and dst routers.
		std::vector<std::tuple<int,int,int>> flat_next_hop_list;
		for (std::tuple<int,int,int> next_hop : valid_next_hops) {
			for (int vc = std::get<1>(next_hop); vc <= std::get<2>(next_hop); vc++) {
				flat_next_hop_list.push_back(std::make_tuple(std::get<0>(next_hop), vc, vc));
			}
		}
		selected_hop = flat_next_hop_list[hash_function(f->entropy, rid, flat_next_hop_list.size())];
	}
	return selected_hop;
}
