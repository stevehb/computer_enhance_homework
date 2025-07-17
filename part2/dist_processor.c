//
// Created by stevehb on 12-Jun-25.
//

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "common_funcs.h"
#include "json_parser.h"
#include "tempo.h"

int main(int argc, char** argv) {
    tempo_startProfile("DIST_PROC");
    tempo_startBlock("startup");
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    char jsonFilename[FILENAME_LEN] = { 0 };
    char distFilename[FILENAME_LEN] = { 0 };

    bool hasJson = getParamValue_str(argc, argv, 1, jsonFilename, FILENAME_LEN);
    bool hasDist = getParamValue_str(argc, argv, 2, distFilename, FILENAME_LEN);

    if (!hasJson) {
        const char* progName = basename(argv[0]);
        fprintf(stdout, "Usage: %s jsonFilename [distFilename]\n", progName);
        fprintf(stdout, "  jsonFilename    generated JSON file with coordinate pairs\n");
        fprintf(stdout, "  distFilename    generated distances file for validation\n");
        exit(0);
    }
    tempo_stopBlock("startup");

    printf("Reading %s\n", jsonFilename);
    JsonFile jsonFile = json_parseFile(jsonFilename);
    u64 jsonFileSize = jsonFile.fileSize;
    u64 jsonElementCount = jsonFile.elementCount;

    tempo_startBlock("dist_fileMap");
    FileState distFile = { 0 };
    if (hasDist) {
        distFile = mmapFile(distFilename);
    }
    tempo_stopBlock("dist_fileMap");

    bool isInPairs = false;
    f64 lng0 = NAN, lat0 = NAN, lng1 = NAN, lat1 = NAN;
    u64 pairsProcessed = 0, totalPairsCount = 0;
    f64 accumCoef = 1.0;
    f64 calcAccum = 0.0;
    f64 maxDistDrift = 0.0;
    u64 distDriftCount = 0;
    u64 maxDistPairIdx = 0;
    f64 distCheckFinal = 0.0;
    u64 distCheckElapsed = 0;
    tempo_startBlock("dist_calc");
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
                }
            }
        }
    }
    tempo_stopBlock("dist_calc");
    if (hasDist) {
        u64 lastOffset = distFile.size - sizeof(f64);
        memcpy(&distCheckFinal, distFile.data + lastOffset, sizeof(f64));
    }

    if (distDriftCount > 0) {
        printf("WARNING: Found %llu distance errors greater than DBL_EPSILON (%E): max error %E at pair %llu\n", distDriftCount, (f64) DBL_EPSILON, maxDistDrift, maxDistPairIdx);
    }

    tempo_startBlock("cleanup");
    if (hasDist) {
        munmapFile(&distFile);
    }
    json_freeFile(&jsonFile);
    tempo_stopBlock("cleanup");

    printf("Read and parsed %llu bytes in %s: %llu elements\n", jsonFileSize, jsonFilename, jsonElementCount);
    printf("Calculated%s distance for %llu coordinate pairs.\n", hasDist ? " and checked" : "", pairsProcessed);
    printf("Final sum:   %.16f\n", calcAccum);
    if (hasDist) {
        printf("Checked sum: %.16f\n", distCheckFinal);
    }
    if (maxDistDrift > 0.0) {
        printf("Max pair error: %.16f at pair index %llu\n", maxDistDrift, maxDistPairIdx);
    }
    printf("\n");

    tempo_stopProfile();
    tempo_printProfile();

    return 0;
}
