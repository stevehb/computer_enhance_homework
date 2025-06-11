//
// Created by stevehb on 11-Jun-25.
//

#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include "types.h"

#define DEG2RAD_FACTOR  0.017453292519943295769
#define RAD2DEG_FACTOR 57.295779513082320877

#define DEG2RAD(deg) ((f64)(deg) * DEG2RAD_FACTOR)
#define RAD2DEG(rad) ((f64)(rad) * RAD2DEG_FACTOR)

f64 referenceHaversineDistance(f64 lng0, f64 lat0, f64 lng1, f64 lat1, f64 rad);

#endif //COMMON_FUNCS_H
