
#include "timer.h"
#include "types.h"
#include <stdio.h>

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>

u64 getOsTimerFreq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}
u64 readOsTimer(void) {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

#else
#include <x86intrin.h>
#include <sys/time.h>

u64 getOsTimerFreq(void) {
    return 1000000;  // Known microseconds
}
u64 readOsTimer(void) {
    struct timerval value;
    gettimeofday(&value, 0);
    u64 result = (getOsTimerFreq() * (u64)value.tv_sec) + (u64)value.tv_usec;
    return result;
}

#endif

u64 readCpuTimer(void) {
    return __rdtsc();
}

u64 estimateCpuFreq(u64 testDurationMillis) {
    u64 osFreq = getOsTimerFreq();
    f64 ofSec = (f64) testDurationMillis / 1000.0;
    u64 testDuration = (u64)(ofSec * (f64)osFreq);
    u64 cpuStart = readCpuTimer();
    u64 osStart = readOsTimer();
    u64 osLast = readOsTimer();
    u64 osElapsed = osLast - osStart;
    u64 cpuStop = readCpuTimer();
    while (osElapsed < testDuration) {
        osLast = readOsTimer();
        cpuStop = readCpuTimer();
        osElapsed = osLast - osStart;
    }
    u64 cpuElapsed = cpuStop - cpuStart;
    u64 cpuFreq = (u64)((f64)cpuElapsed / ofSec);
    return cpuFreq;

    // printf("CPU_FREQ: testDurationMillis:   %llu\n", testDurationMillis);
    // printf("CPU_FREQ: testDuration:         %llu\n", testDuration);
    // printf("CPU_FREQ: osStart:              %llu\n", osStart);
    // printf("CPU_FREQ: osLast:               %llu\n", osLast);
    // printf("CPU_FREQ: osElapsed:            %llu\n", osElapsed);
    // printf("CPU_FREQ: cpuStart:             %llu\n", cpuStart);
    // printf("CPU_FREQ: cpuStop:              %llu\n", cpuStop);
    // printf("CPU_FREQ: cpuElapsed:           %llu\n", cpuElapsed);
    // printf("CPU_FREQ: cpuFreq:              %llu (%.3fGHz)\n", cpuFreq, ((f64)cpuFreq / 1000000000.0));
}
