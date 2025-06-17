//
// Created by stevehb on 11-Jun-25.
//

#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include <stdbool.h>
#include <time.h>

#include "types.h"

#ifdef _WIN32
typedef void* HANDLE;  // Pre-define handle so we don't need windows.h here
#endif

typedef struct {
#ifdef _WIN32
    HANDLE file;
    HANDLE mapping;
#else
    int fd;
#endif

    char* data;
    u64 size;
    u64 position;
} FileState;

extern const f64 EARTH_RAD;

#define FILENAME_LEN    256

#define DEG2RAD_FACTOR  0.017453292519943295769
#define RAD2DEG_FACTOR 57.295779513082320877

#define DEG2RAD(deg) ((f64)(deg) * DEG2RAD_FACTOR)
#define RAD2DEG(rad) ((f64)(rad) * RAD2DEG_FACTOR)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

f64 getElapsedMillis(struct timespec start, struct timespec end);
void makeFilenames(char* jsonFilename, char* distFilename, u32 buffSize, u64 pairCount, u32 clusterCount);
bool getParamValue_str(int argc, char** argv, u32 position, char* buff, u32 buffSize);
bool getParamValue_u32(int argc, char** argv, const char* name, u32* out_value);
bool getParamValue_u64(int argc, char** argv, const char* name, u64* out_value);
f64 referenceHaversineDistance(f64 lng0, f64 lat0, f64 lng1, f64 lat1, f64 rad);
FileState mmapFile(const char* filename);
void munmapFile(FileState* state);

#endif //COMMON_FUNCS_H
