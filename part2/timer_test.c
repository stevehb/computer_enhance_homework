//
// Created by stevehb on 15-Jul-25.
//


#include "timer.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    u64 durationMillis = 1000;
    if (argc >= 2) {
        durationMillis = strtol(argv[1], NULL, 10);
    }
    printf("Measuring CPU frequency for %llums...\n", durationMillis);
    u64 cpuFreq = estimateCpuFreq(durationMillis);
    printf("    CPU Freq: %llu (%.3fGHz)\n", cpuFreq, (f64)cpuFreq / 1000000000.0);

    return 0;
}
