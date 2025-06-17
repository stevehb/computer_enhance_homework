//
// Created by stevehb on 11-Jun-25.
//

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

typedef float       f32;
typedef double      f64;
typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;
typedef __int128_t  s128;
typedef __uint128_t u128;


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


#endif //TYPES_H
