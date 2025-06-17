//
// Created by stevehb on 12-Jun-25.
//

#include <float.h>
#include <libgen.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "common_funcs.h"
#include "json_parser.h"

int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    char jsonFilename[FILENAME_LEN] = { 0 };
    char distFilename[FILENAME_LEN] = { 0 };

    bool hasJson = getParamValue_str(argc, argv, 1, jsonFilename, FILENAME_LEN);
    bool hasDist = getParamValue_str(argc, argv, 2, distFilename, FILENAME_LEN);

    if (!hasJson) {
        const char* progName = basename((char*) argv[0]);
        fprintf(stdout, "Usage: %s jsonFilename [distFilename]\n", progName);
        fprintf(stdout, "  jsonFilename    generated JSON file with coordinate pairs\n");
        fprintf(stdout, "  distFilename    generated distances file for validation\n");
        exit(0);
    }

    struct timespec parseStart, parseEnd;
    clock_gettime(CLOCK_MONOTONIC, &parseStart);
    JsonFile jsonFile = json_parseFile(jsonFilename);
    clock_gettime(CLOCK_MONOTONIC, &parseEnd);
    {
        f64 parsedElapsed = getElapsedMillis(parseStart, parseEnd);
        u64 elementBytesUsed = sizeof(JsonElement) * jsonFile.elementCount;
        u64 elementBytesCapacity = sizeof(JsonElement) * jsonFile.elementCapacity;
        u64 stringBytesUsed = jsonFile.stringBuffUsed;
        u64 stringBytesCapacity = jsonFile.stringBuffCapacity;
        printf("PARSE: Parsed %llu bytes, %llu JSON elements; memory: %llu / %llu bytes for elements, and %llu / %llu bytes for strings\n",
            jsonFile.fileSize, jsonFile.elementCount, elementBytesUsed, elementBytesCapacity, stringBytesUsed, stringBytesCapacity);
        printf("PARSE: Elapsed %.3fms\n", parsedElapsed);
    }

    // printf("Finished parsing file: %llu elements and %llu bytes of strings\n", jsonFile.elementCount, jsonFile.stringBuffUsed);
    // char buff[256] = { 0 };
    // for (u64 i = 0; i < jsonFile.elementCount; i++) {
    //     JsonElement* el = &jsonFile.elements[i];
    //     printf("EL[%3llu] %s\n", i, json_getElementStr(&jsonFile, el, buff, 256));
    // }

    FileState distFile = { 0 };
    if (hasDist) {
        distFile = mmapFile(distFilename);
    }

    struct timespec calcStart, calcEnd;
    clock_gettime(CLOCK_MONOTONIC, &calcStart);
    bool isInPairs = false;
    f64 lng0 = NAN, lat0 = NAN, lng1 = NAN, lat1 = NAN;
    u64 pairsProcessed = 0, totalPairsCount = 0;
    f64 accumCoef = 1.0;
    f64 calcAccum = 0.0;
    f64 maxDistDrift = 0.0;
    u64 distDriftCount = 0;
    u64 maxDistPairIdx = 0;
    for (u64 i = 0; i < jsonFile.elementCount; i++) {
        JsonElement el = jsonFile.elements[i];
        if (el.type == JSON_ARRAY_BEGIN && strcmp(jsonFile.stringBuff + el.nameOffset, "pairs") == 0) {
            isInPairs = true;
            totalPairsCount = el.container.childCount;
            accumCoef = 1.0 / (f64)totalPairsCount;
            continue;
        }
        if (el.type == JSON_ARRAY_END && strcmp(jsonFile.stringBuff + el.nameOffset, "pairs") == 0) {
            isInPairs = false;
            continue;
        }
        if (el.type == JSON_NUMBER && isInPairs) {
            if (strcmp(jsonFile.stringBuff + el.nameOffset, "lng0") == 0) {
                lng0 = el.number.value;
            } else if (strcmp(jsonFile.stringBuff + el.nameOffset, "lat0") == 0) {
                lat0 = el.number.value;
            } else if (strcmp(jsonFile.stringBuff + el.nameOffset, "lng1") == 0) {
                lng1 = el.number.value;
            } else if (strcmp(jsonFile.stringBuff + el.nameOffset, "lat1") == 0) {
                lat1 = el.number.value;
            }
            continue;
        }
        if (el.type == JSON_OBJECT_END && isInPairs) {
            pairsProcessed++;
            if (isnan(lng0) || isnan(lat0) || isnan(lng1) || isnan(lat1)) {
                fprintf(stderr, "ERROR: Missing numbers for pair %llu: (lng0=%.f,lat0=%f), (lng1=%f,lat1=%f)\n", totalPairsCount, lng0, lat0, lng1, lat1);
                exit(1);
            }
            f64 calcDist = referenceHaversineDistance(lng0, lat0, lng1, lat1, EARTH_RAD);
            calcAccum += calcDist * accumCoef;
            lng0 = lat0 = lng1 = lat1 = NAN;
            if (hasDist) {
                f64 knownDist = 0.0;
                memcpy(&knownDist, distFile.data + distFile.position, sizeof(f64));
                distFile.position += sizeof(f64);
                f64 diff = fabs(knownDist - calcDist);
                if (diff >= (DBL_EPSILON * 1.0)) {
                    distDriftCount++;
                    if (diff > maxDistDrift) {
                        maxDistDrift = diff;
                        maxDistPairIdx = pairsProcessed;
                    }
                    // printf("WARNING: Distances differ:\n");
                    // printf("[%llu]    calcDist:  %.16f\n", pairsProcessed, calcDist);
                    // printf("[%llu]    knownDist: %.16f\n", pairsProcessed, knownDist);
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &calcEnd);
    f64 calcElapsed = getElapsedMillis(calcStart, calcEnd);

    if (distDriftCount > 0) {
        printf("WARNING: Found %llu distance errors: max error %.16f at pair %llu\n", distDriftCount, maxDistDrift, maxDistPairIdx);
        printf("         DBL_EPSILON: %.16f\n", DBL_EPSILON);
    }


    printf("CALC: Processed %llu of %llu coordinate pairs.\n", pairsProcessed, totalPairsCount);
    printf("CALC: Elapsed %.3fms\n", calcElapsed);
    printf("    calculated sum:   %.16f\n", calcAccum);
    if (hasDist) {
        f64 knownAccum = 0.0;
        u64 lastOffset = distFile.size - sizeof(f64);
        memcpy(&knownAccum, distFile.data + lastOffset, sizeof(f64));
        printf("    pre-computed sum: %.16f\n", knownAccum);
    }

    if (hasDist) {
        munmapFile(&distFile);
    }
    json_freeFile(&jsonFile);
    return 0;
}
