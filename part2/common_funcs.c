//
// Created by stevehb on 11-Jun-25.
//

#include <math.h>

#include "types.h"
#include "common_funcs.h"

static f64 sqr(f64 f) {
    return f * f;
}

f64 referenceHaversineDistance(f64 lng0, f64 lat0, f64 lng1, f64 lat1, f64 rad) {
    f64 dLat = DEG2RAD(lat1 - lat0);
    f64 dLng = DEG2RAD(lng1 - lng0);
    lat0 = DEG2RAD(lat0);
    lat1 = DEG2RAD(lat1);
    f64 a = sqr(sin(dLat / 2.0)) + cos(lat0) * cos(lat1) * sqr(sin(dLng / 2.0));
    f64 c = 2.0 * asin(sqrt(a));
    return rad * c;
}
