#pragma once
#include "routing_function.hpp"
#include "selection_function.hpp"
#include <vector>
#include <map>
#include <tuple>
#include <utility>  
#include <vector>

class CDG {
public:
    CDG(const std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>>& router_list, int n_routers, int n_vcs);
	void increase_vcs(int number_of_additional_vcs); 																	// Increases the number of VCs in the CDG by adding new nodes and edges accordingly
	void insert_path(std::vector<std::tuple<int,int,int>>& path);														// Path given as (src_rid, dst_rid, vc)-tuples
	void remove_path(int path_id);																						// Path given as internal path ID
	void remove_path(std::vector<std::tuple<int,int,int>>& path);														// Path given as (src_rid, dst_rid, vc)-tuples
	std::vector<std::vector<std::tuple<int,int,int>>> identify_one_cycle_and_remove_min_number_of_paths_to_break_it();	// Returns the paths that were removed to break the cycle
    bool is_cyclic() const;
private:
	int _path_id_counter; 																								// Counter to assign unique IDs to paths
	int _n_routers; 																									// Number of routers in the CDG
	int _n_vcs; 																										// Number of virtual channels in the CDG
	const std::vector<std::map<int,std::map<int,std::tuple<int,int,int>>>> _router_list;
    std::map<std::tuple<int,int,int>, std::vector<std::tuple<int,int,int>>> _cdg_adj_list; 								// Adjacency list where virtual channels, i.e., (src_rid, dst_rid, vc)-tuples are nodes
	std::map<int, std::vector<std::tuple<int,int,int>>> _paths; 														// List of paths, where each path is a vector of (src_rid, dst_rid, vc)-tuples. Index is the internal path ID.
	std::map<std::pair<std::tuple<int,int,int>, std::tuple<int,int,int>>, std::vector<int>> _edge_to_paths;				// Maps an edge (from_node, to_node) to the list of path IDs that use this edge
};

