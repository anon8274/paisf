#include "topology_hash.hpp"
#include <functional>

namespace router_hash
{
	namespace {
		template <class T>
		inline void hash_combine(std::size_t& seed, const T& value) {
			std::hash<T> hasher;
			seed ^= hasher(value)
				  + 0x9e3779b97f4a7c15ULL
				  + (seed << 6)
				  + (seed >> 2);
		}
	}

	std::size_t hash_router_list(const RouterList& router_list) {
		std::size_t seed = 0;
		for (const auto& outer_map : router_list){
			for (const auto& [k1, inner_map] : outer_map){
				hash_combine(seed, k1);
				for (const auto& [k2, tpl] : inner_map){
					hash_combine(seed, k2);
					hash_combine(seed, std::get<0>(tpl));
					hash_combine(seed, std::get<1>(tpl));
					hash_combine(seed, std::get<2>(tpl));
				}
			}
		}
		return seed;
	}
}
