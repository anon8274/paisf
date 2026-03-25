#include "hash_function.hpp"
#include <algorithm>
#include <cstdint>
#include <cassert>

int n_entropy_bits = 12345;				// Dummy value, replace with actual bit width from anynet file
int n_router_id_bits = 12345;			// Dummy value, replace with actual bit width from anynet file

// Total Gates: w * (log2(w) + log2(n) + 1), where w = max(n_entropy_bits, n_router_id_bits) + 2
// Parameter "entropy": The entropy value that a given packet carries
// Parameter "router_id": The unique ID of the router where this hash function is instantiated
// Parameter "n": The number of valid (physical_channel, virtual_channel) pairs to choose from
// Returns: An integer in the range [0, n-1]
int hash_function(int entropy, int router_id, int n) {
    // Make sure that n_entropy_bits and n_router_id_bits are defined and not left as dummy values
    assert(n_entropy_bits != 12345 && n_router_id_bits != 12345);
    // Sanity check
    assert(n > 0);

    // Compute the internal register width w in hardware (in this C++ code we emulate it with masks)
    int w = std::max(n_entropy_bits, n_router_id_bits) + 2;
    uint32_t w_mask = (w >= 32) ? ~0u : (1u << w) - 1;

    // Mask the entropy to emulate that we only have n_entropy_bits many bits of it. 
    uint32_t z = static_cast<uint32_t>(entropy) & ((n_entropy_bits >= 32) ? ~0u : (1u << n_entropy_bits) - 1);

    // XOR with the router-ID to mix in the router-specific information
	// HARDWARE COST: w gates
    z ^= static_cast<uint32_t>(router_id);
    z &= w_mask;

	// Multiply by a constant to further mix the bits, this is where most of the randomness comes from
	// HARDWARE COST: w * log2(w) gates
	z *= 0x9E3779B9u;
    z &= w_mask;

    // We implement the modulo-n operation using a wide multiplication followed by a right-shift
	// HARDWARE COST: w * log2(n) gates
    uint64_t wide_product = static_cast<uint64_t>(z) * n;
    int result = wide_product >> w;

	// Return the final result in the range [0, n-1]
    return result;
}

