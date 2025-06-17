//
// Created by stevehb on 12-Jun-25.
//

#include <float.h>
#include <libgen.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    JsonFile jsonFile = json_parseFile(jsonFilename);
    {
        u64 elementBytesUsed = sizeof(JsonElement) * jsonFile.elementCount;
        u64 elementBytesCapacity = sizeof(JsonElement) * jsonFile.elementCapacity;
        u64 stringBytesUsed = jsonFile.stringBuffUsed;
        u64 stringBytesCapacity = jsonFile.stringBuffCapacity;
        printf("Parsed %llu bytes for %llu JSON elements. Using %llu / %llu bytes for elements, and %llu / %llu bytes for strings\n",
            jsonFile.fileSize, jsonFile.elementCount, elementBytesUsed, elementBytesCapacity, stringBytesUsed, stringBytesCapacity);
    }

    // printf("Finished parsing file: %llu elements and %llu bytes of strings\n", jsonFile.elementCount, jsonFile.stringBuffUsed);
    // char buff[256] = { 0 };
    // for (u64 i = 0; i < jsonFile.elementCount; i++) {
    //     JsonElement* el = &jsonFile.elements[i];
    //     printf("EL[%3llu] %s\n", i, json_getElementStr(el, buff, 256));
    // }

    FileState distFile = { 0 };
    if (hasDist) {
        distFile = mmapFile(distFilename);
    }

    bool isInPairs = false;
    f64 lng0 = NAN, lat0 = NAN, lng1 = NAN, lat1 = NAN;
    u64 pairsProcessed = 0, totalPairsCount = 0;
    f64 accumCoef = 1.0;
    f64 calcAccum = 0.0;
    for (u64 i = 0; i < jsonFile.elementCount; i++) {
        JsonElement el = jsonFile.elements[i];
        if (el.type == JSON_ARRAY_BEGIN && strcmp(el.name, "pairs") == 0) {
            isInPairs = true;
            totalPairsCount = el.container.childCount;
            accumCoef = 1.0 / (f64)totalPairsCount;
            continue;
        }
        if (el.type == JSON_ARRAY_END && strcmp(el.name, "pairs") == 0) {
            isInPairs = false;
            continue;
        }
        if (el.type == JSON_NUMBER && isInPairs) {
            if (strcmp(el.name, "lng0") == 0) {
                lng0 = el.number.value;
            } else if (strcmp(el.name, "lat0") == 0) {
                lat0 = el.number.value;
            } else if (strcmp(el.name, "lng1") == 0) {
                lng1 = el.number.value;
            } else if (strcmp(el.name, "lat1") == 0) {
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
                if (fabs(knownDist - calcDist) >= (DBL_EPSILON * 2.0)) {
                    printf("WARNING: Distances differ:\n");
                    printf("[%llu]    calcDist:  %.16f\n", totalPairsCount, calcDist);
                    printf("[%llu]    knownDist: %.16f\n", totalPairsCount, knownDist);
                }
            }
        }
    }
    printf("Processed %llu of %llu coordinate pairs.\n", pairsProcessed, totalPairsCount);
    printf("Calculated sum: %.16f\n", calcAccum);
    if (hasDist) {
        f64 knownAccum = 0.0;
        u64 lastOffset = distFile.size - sizeof(f64);
        memcpy(&knownAccum, distFile.data + lastOffset, sizeof(f64));
        printf("Known sum:      %.16f\n", knownAccum);
    }

    if (hasDist) {
        munmapFile(&distFile);
    }
    json_freeFile(&jsonFile);
    return 0;
}
