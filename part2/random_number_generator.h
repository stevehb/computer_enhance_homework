//
// Created by stevehb on 11-Jun-25.
//

#ifndef RANDOM_NUMBER_GENERATOR_H
#define RANDOM_NUMBER_GENERATOR_H

#include "types.h"

void xoshiro_seed(u32 seed);
u32 rand_u32(u32 min, u32 max);
f32 rand_f32(f32 min, f32 max);
f64 rand_f64(f64 min, f64 max);

#endif //RANDOM_NUMBER_GENERATOR_H
