#pragma once

#include <vector>
#include <map>
#include <tuple>
#include <cstddef>

namespace router_hash {
	using RouterList = std::vector<std::map<int,std::map<int, std::tuple<int,int,int>>>>;
	std::size_t hash_router_list(const RouterList& router_list);
}
