#pragma once

#include <cstdint>
#include <cassert>
#include <cmath>

int hash_function(int entropy, int rid, int n);
extern int n_entropy_bits;
extern int n_router_id_bits;

