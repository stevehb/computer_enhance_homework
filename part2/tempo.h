
#ifndef TEMPO_H
#define TEMPO_H

#include "types.h"

#define TEMPO_MAX_BLOCKS 256

typedef struct TempoBlock {
    u32 depth;
    const char* label;
    u64 startTicks, stopTicks;
} TempoBlock;

void tempo_startProfile(const char* label);
void tempo_stopProfile(void);
void tempo_printProfile(void);
void tempo_startBlock(const char* label);
void tempo_stopBlock(const char* label);
u64 tempo_estimateCpuFreq(u64 testDurationMillis);

#define tempo_startFunc tempo_startBlock(__func__)
#define tempo_stopFunc tempo_stopBlock(__func__)

#endif  // TEMPO_H
