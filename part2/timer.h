
#ifndef TIMER_H
#define TIMER_H

#include "types.h"

u64 getOsTimerFreq(void);
u64 readOsTimer(void);
u64 readCpuTimer(void);
u64 estimateCpuFreq(u64 testDurationMillis);

#endif  // TIMER_H
