/* 김현수, Redpill Day 22. See README.md for contracts and adaptations. */
#ifndef REDPILL_TIMER_H
#define REDPILL_TIMER_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

typedef void (*rp22_TimerExpiredCallback)(unsigned int timer_id);

/* Single global scheduler. Callback must be non-NULL and must not reenter.
 * Initialize only with no active timers. Set unique IDs; 0ms is rejected.
 * Tick advances simulated time by 1ms, not wall-clock time. Not ISR safe. */
void rp22_InitTimerScheduler(rp22_TimerExpiredCallback callback);

void rp22_SetTimer(unsigned int id, unsigned int ms);

void rp22_Tick(void);

int rp22_demo(void);

#endif
