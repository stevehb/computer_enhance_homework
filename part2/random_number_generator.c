//
// Created by stevehb on 11-Jun-25.
//

#include <float.h>

#include "types.h"
#include "random_number_generator.h"

static u32 xoshiro_primarySeed = 0;
static u32 xoshiro_state[4] __attribute__((aligned(16))) = {1, 2, 3, 4};

static u32 xoshiro_rotl(u32 x, int k) {
    return (x << k) | (x >> (32 - k));
}

static u32 xoshiro128() {
    u32 result = xoshiro_rotl(xoshiro_state[1] * 5, 7) * 9;
    u32 t = xoshiro_state[1] << 9;
    xoshiro_state[2] ^= xoshiro_state[0];
    xoshiro_state[3] ^= xoshiro_state[1];
    xoshiro_state[1] ^= xoshiro_state[2];
    xoshiro_state[0] ^= xoshiro_state[3];
    xoshiro_state[2] ^= t;
    xoshiro_state[3] = xoshiro_rotl(xoshiro_state[3], 11);
    return result;
}

void xoshiro_seed(u32 seed) {
    xoshiro_primarySeed = seed;

    // Scramble seed bits using the LCG constants
    xoshiro_state[0] = seed = seed * 1664525u + 1013904223u;
    xoshiro_state[1] = seed = seed * 1664525u + 1013904223u;
    xoshiro_state[2] = seed = seed * 1664525u + 1013904223u;
    xoshiro_state[3] = seed = seed * 1664525u + 1013904223u;

    // Warmup the entropy
    for(int i = 0; i < 8; i++) { xoshiro128(); }
}

// Inclusive of `min` and `max`
u32 rand_u32(u32 min, u32 max) {
    u64 range = (u64)max - (u64)min + 1;
    u64 r = ((u64)xoshiro128() * range) >> 32;
    return (u32)r + min;
}

f32 rand_f32(f32 min, f32 max) {
    // Use all 32 bits but scale to [0,1] range
    u32 ri = xoshiro128();
    f32 r = (f32)ri / (f32)UINT32_MAX;
    return min + r * (max - min);
}

f64 rand_f64(f64 min, f64 max) {
    // Combine two 32-bit values for full 64-bit precision
    u64 ri = ((u64)xoshiro128() << 32) | xoshiro128();
    f64 r = (f64)ri / (f64)UINT64_MAX;
    return min + r * (max - min);
}
