#pragma once
#include "selection_function.hpp"
#include <vector>
#include <tuple>

// Forward declaration of Flit to avoid circular dependency
class Flit; 

class deterministic_selection : public selection_function {
public:
    deterministic_selection() = default;
    std::tuple<int,int,int>
    select(int rid, std::vector<std::tuple<int,int,int>> valid_next_hops, const Flit* f) const override;
};
