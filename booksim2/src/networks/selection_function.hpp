#pragma once
#include <vector>
#include <tuple>

// Forward declaration of Flit to avoid circular dependency
class Flit; 

class selection_function {
public:
    virtual ~selection_function() = default;
    virtual std::tuple<int,int,int> select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit *f) const = 0;
	mutable int storage_overhead = 0;
};

