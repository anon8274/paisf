#include "xy_routing.hpp"
#include <vector>
#include <tuple>
#include <cmath>
#include <iostream>
#include <cassert>

// Deadlock-free: YES
// Minimal: YES
// Deterministic: YES
// Description: Route in X-direction first, then route in Y-direction.
// Settings: Shortest paths w.r.t. hops vs. shortest paths w.r.t. latency
// WARNING: This implementation only works for 2D mesh topologies of for topologies that contain a 2D mesh with potentially additional links (e.g., express links), which are not used by this routing algorithm.
// WARNING: This implementation assumes that the router IDs are numbered in row-major order for a 2D mesh topology.
std::vector<std::tuple<int,int,int>> xy_routing::route(std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> router_list, int n_routers, int n_nodes, int n_vsc, int src_rid, int cur_rid, int cur_vc, int dst_nid, int dst_rid, int in_port, int flit_id) const {
	// This only works for 2D mesh topologies with a square number of routers
	int sqrt_n = int(std::sqrt(n_routers));
	if (sqrt_n * sqrt_n != n_routers) {
		std::cout << "Error: XY routing only works for 2D mesh topologies with a square number of routers. Number of routers: " << n_routers << std::endl;
		assert(false);
	}
	int out_port = -1;
	// If we already reached the destination router, then route to the destination node
	if (cur_rid == dst_rid) {
		// Identify the output port to the destination node
		out_port = std::get<0>(router_list[0][dst_rid][dst_nid]);
	// We are not yet at the destination router
	} else {
		int cur_row = cur_rid / sqrt_n;
		int cur_col = cur_rid % sqrt_n;
		int dst_row = dst_rid / sqrt_n;
		int dst_col = dst_rid % sqrt_n;
		int dir = 0;
		int nxt_rid = -1;
		// We are not yet in the correct column: Route in X-direction
		if (cur_col != dst_col){
			dir = (dst_col > cur_col) ? 1 : -1;
			nxt_rid = cur_rid + dir;
		// We are in the correct column: Route in Y-direction
		} else {
			dir = (dst_row > cur_row) ? 1 : -1;
			nxt_rid = cur_rid + dir * sqrt_n;
		}
		out_port = std::get<0>(router_list[1][cur_rid][nxt_rid]);
	}
	// Return the valid next hop (output port, min VC, max VC)
	return {std::make_tuple(out_port, 0, n_vsc-1)};
}



