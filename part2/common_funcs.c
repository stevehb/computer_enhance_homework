//
// Created by stevehb on 11-Jun-25.
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windef.h>
#include <winbase.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "types.h"
#include "common_funcs.h"

const f64 EARTH_RAD = 6372.8;

f64 getElapsedMillis(struct timespec start, struct timespec end) {
    f64 elapsed = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed += (end.tv_nsec - start.tv_nsec) / 1000000.0;
    return elapsed;
}

void makeFilenames(char* jsonFilename, char* distFilename, u32 buffSize, u64 pairCount, u32 clusterCount) {
    snprintf(jsonFilename, buffSize, "data-%llu-%u-coords.json", pairCount, clusterCount);
    snprintf(distFilename, buffSize, "data-%llu-%u-dist.f64", pairCount, clusterCount);
}

/// position == 1 is the first argument, etc
bool getParamValue_str(int argc, char** argv, u32 position, char* buff, u32 buffSize) {
    if (position >= argc) { return false; }
    char* arg = argv[position];
    snprintf(buff, buffSize, "%s", arg);
    return true;
}

bool getParamValue_u32(int argc, char** argv, const char* name, u32* out_value) {
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

bool getParamValue_u64(int argc, char** argv, const char* name, u64* out_value) {
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

FileState mmapFile(const char* filename) {
    FileState state = { 0 };
#ifdef _WIN32
    state.file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (state.file == INVALID_HANDLE_VALUE) goto error;
    DWORD sizeHi = 0;
    DWORD sizeLo = GetFileSize(state.file, &sizeHi);
    state.size = ((u64) sizeHi << 32) | sizeLo;
    state.mapping = CreateFileMapping(state.file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!state.mapping) goto error;
    state.data = MapViewOfFile(state.mapping, FILE_MAP_READ, 0, 0, 0);
    if (!state.data) goto error;
#else
    state.fd = open(filename, O_RDONLY);
    if (state.fd < 0) goto error;
    struct stat st = { 0 };
    if (fstat(state.fd, &st) < 0) goto error;
    state.size = st.st_size;
    state.data = mmap(NULL, state.size, PROT_READ, MAP_PRIVATE, state.fd, 0);
    if (state.data == MAP_FAILED) goto error;
#endif
    return state;

    error:
        fprintf(stderr, "ERROR: Failed to memory-map file %s\n", filename);
    munmapFile(&state);
    exit(1);
}
void munmapFile(FileState* state) {
    if (state == NULL) return;

#ifdef _WIN32
    if (state->data) {
        UnmapViewOfFile(state->data);
    }
    if (state->mapping) {
        CloseHandle(state->mapping);
    }
    if (state->file) {
        CloseHandle(state->file);
    }
#else
    if (state->data && state->data != MAP_FAILED) {
        munmap(state->data, state->size);
    }
    if (state->fd >= 0) {
        close(state->fd);
    }
#endif
}


