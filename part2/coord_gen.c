//
// Created by stevehb on 11-Jun-25.
//

#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "common_funcs.h"
#include "random_number_generator.h"

const f64 EARTH_RAD = 6372.8;

const f64 MIN_LNG = -180.0;
const f64 MAX_LNG = 180.0;
const f64 MIN_LAT = -90.0;
const f64 MAX_LAT = 90.0;

f64 wrapLng(f64 f) {
    if (f < MIN_LNG) f += 360.0;
    if (f > MAX_LNG) f -= 360.0;
    return f;
}
f64 clampLat(f64 f) {
    if (f < MIN_LAT) return MIN_LAT;
    if (f > MAX_LAT) return MAX_LAT;
    return f;
}

static bool getParamValue_u32(int argc, char** argv, const char* name, u32* out_value);
static bool getParamValue_u64(int argc, char** argv, const char* name, u64* out_value);

int main(int argc, char** argv) {
    u32 seed = 1000;
    u64 pairCount = 5;
    u32 clusterCount = 4;
    char jsonFilename[256] = { 0 }, distFilename[256] = { 0 };

    getParamValue_u32(argc, argv, "-seed", &seed);
    getParamValue_u64(argc, argv, "-pairs", &pairCount);
    getParamValue_u32(argc, argv, "-clusters", &clusterCount);

    xoshiro_seed(seed);
    snprintf(jsonFilename, sizeof(jsonFilename), "data-%llu-%u-coords.json", pairCount, clusterCount);
    snprintf(distFilename, sizeof(distFilename), "data-%llu-%u-dist.f64", pairCount, clusterCount);

    printf("SEED: %u\n", seed);
    printf("FILENAMES: '%s' and '%s'\n", jsonFilename, distFilename);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    FILE* jsonF = fopen(jsonFilename, "w");
    FILE* distF = fopen(distFilename, "wb");
    fprintf(jsonF, "{\"pairs\":[\n");
    u64 basePairCountPerCluster = pairCount / clusterCount;
    u64 clustersWithRemainder = pairCount % clusterCount;
    f64 accumCoef = 1.0 / (f64)pairCount;
    f64 accum = 0.0;
    u64 total = 0;
    for (u32 clusterIdx = 0; clusterIdx < clusterCount; clusterIdx++) {
        bool isLastCluster = clusterIdx == clusterCount - 1;
        u64 clusterPairCount = basePairCountPerCluster + ((clusterIdx < clustersWithRemainder) ? 1 : 0);
        total += clusterPairCount;
        f64 clusterRad = rand_f64(5.0, 20.0);
        f64 clusterLng = rand_f64(MIN_LNG, MAX_LNG);
        f64 clusterLat = rand_f64(MIN_LAT, MAX_LAT);
        f64 minLng = clusterLng - clusterRad;
        f64 maxLng = clusterLng + clusterRad;
        f64 minLat = clusterLat - clusterRad;
        f64 maxLat = clusterLat + clusterRad;
        char* jsonBuffStart = malloc(clusterPairCount * 128);  // Approx
        char* jsonBuff = jsonBuffStart;
        f64* distBuff = malloc(clusterPairCount * sizeof(f64));
        for (u64 pairIdx = 0; pairIdx < clusterPairCount; pairIdx++) {
            bool isLastPair = pairIdx == clusterPairCount - 1;
            f64 lng0 = rand_f64(minLng, maxLng);
            f64 lat0 = rand_f64(minLat, maxLat);
            f64 lng1 = rand_f64(minLng, maxLng);
            f64 lat1 = rand_f64(minLat, maxLat);
            char commaChr = (isLastCluster && isLastPair) ? ' ' : ',';
            f64 dist = referenceHaversineDistance(lng0, lat0, lng1, lat1, EARTH_RAD);
            jsonBuff += snprintf(jsonBuff, 128, "  {\"lng0\":%.16f,\"lat0\":%.16f,\"lng1\":%.16f,\"lat1\":%.16f}%c\n", lng0, lat0, lng1, lat1, commaChr);
            distBuff[pairIdx] = dist;
            accum += (accumCoef * dist);
        }
        fwrite(jsonBuffStart, jsonBuff-jsonBuffStart, 1, jsonF);
        free(jsonBuffStart);
        fwrite(distBuff, sizeof(f64), clusterPairCount, distF);
        free(distBuff);
    }
    fprintf(jsonF, "]}\n");
    fwrite(&accum, sizeof(accum), 1, distF);
    fclose(distF);
    fclose(jsonF);

    clock_gettime(CLOCK_MONOTONIC, &end);
    f64 elapsed = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed += (end.tv_nsec - start.tv_nsec) / 1000000.0;

    fflush(stdout);
    fflush(stderr);
    printf("ELAPSED: %.3f ms\n", elapsed);
    printf("AVG DISTANCE: %.4f\n", accum);
    fflush(stdout);
    fflush(stderr);
}


static bool getParamValue_u32(int argc, char** argv, const char* name, u32* out_value) {
    for (int argIdx = 1; argIdx < argc; argIdx++) {
        const char* arg = argv[argIdx];
        if (strcmp(arg, name) == 0) {
            if (argIdx + 1 < argc) {
                const char* seedStr = argv[argIdx+1];
                u32 holder = 0;
                int matches = sscanf(seedStr, "%u", &holder);
                if (matches != 1) {
                    fprintf(stderr, "ERROR: the parameter value for `%s %s` does not parse to an integer\n", name, seedStr);
                    exit(0);
                }
                *out_value = holder;
                return true;
            }
        }
    }
    return false;
}
static bool getParamValue_u64(int argc, char** argv, const char* name, u64* out_value) {
    for (int argIdx = 1; argIdx < argc; argIdx++) {
        const char* arg = argv[argIdx];
        if (strcmp(arg, name) == 0) {
            if (argIdx + 1 < argc) {
                const char* seedStr = argv[argIdx+1];
                u64 holder = 0;
                int matches = sscanf(seedStr, "%llu", &holder);
                if (matches != 1) {
                    fprintf(stderr, "ERROR: the parameter value for `%s %s` does not parse to an integer\n", name, seedStr);
                    exit(0);
                }
                *out_value = holder;
                return true;
            }
        }
    }
    return false;
}
