
#include "common_funcs.h"
#include "tempo.h"
#include "types.h"
#include <stdio.h>

static u64 tempo_getOsTimerFreq(void);
static u64 tempo_readOsTimer(void);
static u64 tempo_readCpuTimer(void);

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
static u64 tempo_getOsTimerFreq(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}
static u64 tempo_readOsTimer(void) {
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}
#else
#include <x86intrin.h>
#include <sys/time.h>
static u64 tempo_getOsTimerFreq(void) {
    return 1000000;  // Known microseconds
}
static u64 tempo_readOsTimer(void) {
    struct timerval value;
    gettimeofday(&value, 0);
    u64 result = (getOsTimerFreq() * (u64)value.tv_sec) + (u64)value.tv_usec;
    return result;
}
#endif

static u64 tempo_readCpuTimer(void) {
    return __rdtsc();
}

u64 tempo_estimateCpuFreq(u64 testDurationMillis) {
    u64 osFreq = tempo_getOsTimerFreq();
    f64 ofSec = (f64) testDurationMillis / 1000.0;
    u64 testDuration = (u64)(ofSec * (f64)osFreq);
    u64 cpuStart = tempo_readCpuTimer();
    u64 osStart = tempo_readOsTimer();
    u64 osLast = tempo_readOsTimer();
    u64 osElapsed = osLast - osStart;
    u64 cpuStop = tempo_readCpuTimer();
    while (osElapsed < testDuration) {
        osLast = tempo_readOsTimer();
        cpuStop = tempo_readCpuTimer();
        osElapsed = osLast - osStart;
    }
    u64 cpuElapsed = cpuStop - cpuStart;
    u64 cpuFreq = (u64)((f64)cpuElapsed / ofSec);
    return cpuFreq;
}


static struct {
    u32 currentDepth;
    TempoBlock blocks[TEMPO_MAX_BLOCKS];
    u32 currentBlock;
    u32 nextBlockIdx;
    u64 osTimerStart;
    u64 osTimerStop;
    u64 cpuFreq;
} tempoData;

void tempo_startProfile(const char* label) {
    memset(&tempoData, 0, sizeof(tempoData));
    tempoData.currentDepth = -1;
    tempoData.currentBlock = 0;
    tempoData.nextBlockIdx = 0;
    tempoData.osTimerStart = tempo_readOsTimer();
    tempo_startBlock(label);
}
void tempo_stopProfile(void) {
    u64 stopCpuTicks = tempo_readCpuTimer();
    u64 stopOsTicks = tempo_readOsTimer();
    if (tempoData.currentBlock != 0) {
        fprintf(stderr, "WARNING: end of Tempo period with open blocks: currentBlock=%d\n", tempoData.currentBlock);
    }
    tempoData.blocks[0].stopTicks = stopCpuTicks;
    tempoData.osTimerStop = stopOsTicks;
    u64 osFreq = tempo_getOsTimerFreq();
    u64 osTicks = tempoData.osTimerStop - tempoData.osTimerStart;
    u64 cpuTicks = tempoData.blocks[0].stopTicks - tempoData.blocks[0].startTicks;
    tempoData.cpuFreq = (u64)(((f64) osFreq * (f64) cpuTicks) / (f64) osTicks);
}

void tempo_printProfile(void) {
    if (tempoData.nextBlockIdx == 0) {
        printf("TEMPO: No recorded blocks\n");
        return;
    }
    u32 maxDepth = 0;
    u32 maxLabelLen = 0;
    for (u32 i = 0; i < tempoData.nextBlockIdx; i++) {
        maxDepth = MAX(tempoData.blocks[i].depth, maxDepth);
        maxLabelLen = MAX(strlen(tempoData.blocks[i].label), maxLabelLen);
    }
    u64 totalTicks = tempoData.blocks[0].stopTicks - tempoData.blocks[0].startTicks;
    f64 totalMs = ((f64) totalTicks / (f64) tempoData.cpuFreq) * 1000.0;
    printf("TEMPO: Recorded %llu ticks: %.3fms at %.3fGHz\n",
        totalTicks, totalMs, (f64)tempoData.cpuFreq / 1000000000.0);
    u64 parentStack[TEMPO_MAX_BLOCKS + 1] = { 0 };
    parentStack[0] = totalTicks;
    for (u32 i = 0; i < tempoData.nextBlockIdx; i++) {
        const char* label = tempoData.blocks[i].label;
        u32 depth = tempoData.blocks[i].depth;
        u64 ticks = tempoData.blocks[i].stopTicks - tempoData.blocks[i].startTicks;
        u64 parentTicks = parentStack[depth];
        f64 pctOfParent = ((f64)ticks / (f64)parentTicks) * 100.0;
        f64 blockMs = ((f64) ticks / (f64) tempoData.cpuFreq) * 1000.0;
        if (depth + 1 <= maxDepth) {
            parentStack[depth + 1] = ticks;
        }
        printf("TEMPO: [%3u] %*s%s: elapsed=%llu (%.3fms, %6.3f%%)\n",
            i, depth * 2, "", label, ticks, blockMs, pctOfParent);
    }
}

void tempo_startBlock(const char* label) {
    u64 startTicks = tempo_readCpuTimer();
    if (tempoData.nextBlockIdx >= TEMPO_MAX_BLOCKS) {
        fprintf(stderr, "ERROR: Cannot start profiler block %u: too many profiler blocks!\n", tempoData.nextBlockIdx);
        exit(1);
    }
    tempoData.currentBlock = tempoData.nextBlockIdx++;
    tempoData.currentDepth++;
    tempoData.blocks[tempoData.currentBlock].label = label;
    tempoData.blocks[tempoData.currentBlock].depth = tempoData.currentDepth;
    tempoData.blocks[tempoData.currentBlock].startTicks = startTicks;
    tempoData.blocks[tempoData.currentBlock].stopTicks = 0;
    // printf("DBG: OPEN [%d:%d] %s\n", tempoData.currentBlock, tempoData.blocks[tempoData.currentBlock].depth, label);
}

void tempo_stopBlock(const char* label) {
    u64 stopTicks = tempo_readCpuTimer();
    if (label != NULL) {
        if (strcmp(label, tempoData.blocks[tempoData.currentBlock].label) != 0) {
            fprintf(stderr, "WARNING: Unexpected block closure: expected '%s', found '%s'label\n",
                label, tempoData.blocks[tempoData.currentBlock].label);
        }
    }
    tempoData.blocks[tempoData.currentBlock].stopTicks = stopTicks;
    // printf("DBG: CLOS [%d:%d] %s\n", tempoData.currentBlock, tempoData.blocks[tempoData.currentBlock].depth,tempoData.blocks[tempoData.currentBlock].label);
    tempoData.currentDepth = tempoData.blocks[tempoData.currentBlock].depth - 1;
    for (int idx = tempoData.currentBlock-1; idx >= 0; idx--) {
        if (tempoData.blocks[idx].depth == tempoData.currentDepth) {
            tempoData.currentBlock = idx;
            // printf("DBG:   NEXT [%d:%d] %s\n", tempoData.currentBlock, tempoData.blocks[tempoData.currentBlock].depth, tempoData.blocks[tempoData.currentBlock].label);
            break;
        }
    }
}

