/* 김현수 제출 코드 기반. 원문: originals/day22.md */
#include "timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


// Timer
// - id: unique timer identifier
// - dt: delay relative to the previous timer
// - next: pointer to the next timer
// The sum of dt values from the head to a node gives that timer's
// total remaining time. Tick() only needs to decrement the head.
typedef struct rp22_Timer
{
    unsigned int id;
    unsigned int dt;
    struct rp22_Timer *next;
} rp22_Timer;

// TimerExpiredCallback
// - callback function type for handling one expired timer


// TimerScheduler
// - head: first timer in the delta list
// - tick_count: number of times Tick() has advanced the scheduler
// - allocated_count: number of Timer nodes successfully allocated
// - freed_count: number of Timer nodes freed after expiration
// - on_expired: callback invoked once for each expired timer
typedef struct rp22_TimerScheduler
{
    rp22_Timer *head;
    unsigned int tick_count;
    size_t allocated_count;
    size_t freed_count;
    rp22_TimerExpiredCallback on_expired;
} rp22_TimerScheduler;

// InitTimerScheduler
// - callback: function to call whenever one timer expires
// - requires the existing scheduler to have no active or unfreed timers
void rp22_InitTimerScheduler(rp22_TimerExpiredCallback callback);

// PrintTimer
// - timer: ptr to Timer (const as read only)
// prints the timers from head to tail as linked list
static void PrintTimer(const rp22_Timer *timer);

// SetTimer
// - id: unique timer ID; the caller must ensure that no active timer uses it
// - ms: expiration delay in milliseconds
void rp22_SetTimer(unsigned int id, unsigned int ms);

// Tick
// Advances the scheduler by one tick.
void rp22_Tick(void);

// ReportExpiredTimer
// - timer_id: ID of the timer that expired
static void ReportExpiredTimer(unsigned int timer_id);

// AssertAllTimersFreed
// - verifies that no scheduled or unfreed Timer nodes remain
static void AssertAllTimersFreed(void);

// TimerScheduler
// - head: initialized to NULL
// - tick_count: initialized to 0
// - on_expired: registered later by InitTimerScheduler
static rp22_TimerScheduler scheduler = {
    .head = NULL,
    .tick_count = 0,
    .allocated_count = 0,
    .freed_count = 0,
    .on_expired = NULL,
};

int rp22_demo(void)
{
    printf("=== Day 22: Software Timer (Delta List) ===\n\n");
    rp22_InitTimerScheduler(ReportExpiredTimer);

    rp22_SetTimer(1, 10);
    rp22_SetTimer(2, 5);
    rp22_SetTimer(3, 15);

    printf("\n>> Start Ticking...\n");
    while (scheduler.head != NULL)
    {
        rp22_Tick();
    }
    AssertAllTimersFreed();
    return 0;
}
void rp22_InitTimerScheduler(rp22_TimerExpiredCallback callback)
{
    assert(callback != NULL);
    assert(scheduler.head == NULL);
    assert(scheduler.allocated_count == scheduler.freed_count);

    scheduler.head = NULL;
    scheduler.tick_count = 0;
    scheduler.allocated_count = 0;
    scheduler.freed_count = 0;
    scheduler.on_expired = callback;
}

void rp22_SetTimer(unsigned int id, unsigned int ms)
{
    // CASE: 0 ms
    if (ms == 0)
    {
        printf("Timer %u rejected due to 0 ms.\n", id);
        return;
    }

    // 1. Checkpoint: Allocate memory for new_timer
    rp22_Timer *new_timer = malloc(sizeof(*new_timer));
    if (new_timer == NULL)
    {
        printf("Timer %u allocation failed.\n", id);
        return;
    }
    scheduler.allocated_count += 1;

    // 2. Find its insertion position and calculate its delta
    rp22_Timer *prev_timer = NULL, *next_timer = scheduler.head;
    unsigned int remaining_ms = ms;
    while ((next_timer != NULL) && (remaining_ms >= next_timer->dt))
    {
        remaining_ms -= next_timer->dt;
        prev_timer = next_timer;
        next_timer = next_timer->next;
    }
    // 3. Initialize the new timer
    new_timer->id = id;           // Assign ID
    new_timer->dt = remaining_ms; // Assign dt

    // 4. Adjust the following timer to preserve the delta invariant
    if (next_timer != NULL)
    {
        next_timer->dt -= remaining_ms;
    }

    // 5. Link the new timer into the list
    new_timer->next = next_timer;
    if (prev_timer == NULL)
    {
        scheduler.head = new_timer;
        printf("Timer %u set (%u ms) [Inserted at HEAD]\n", new_timer->id, ms);
    }
    else
    {
        prev_timer->next = new_timer;
        printf("Timer %u set (%u ms) [Inserted in List]\n", new_timer->id, ms);
    }
    // 6. Print the updated timer list
    PrintTimer(scheduler.head);
}

static void PrintTimer(const rp22_Timer *timer)
{
    printf("[Timer List]");
    unsigned int total = 0;
    while (timer)
    {
        total += timer->dt;
        printf(" (ID:%u, dt:%u, total:%u) ->", timer->id, timer->dt, total);
        timer = timer->next;
    }
    printf(" NULL\n");
}

void rp22_Tick(void)
{
    if (scheduler.head == NULL)
    {
        return;
    }

    scheduler.tick_count += 1;
    if (scheduler.head->dt > 0)
    {
        scheduler.head->dt -= 1;
    }

    if (scheduler.head->dt > 0)
    {
        printf("Tick %u: Rem Head dt: %u\n",
               scheduler.tick_count,
               scheduler.head->dt);
        return;
    }

    // CASE: Expired Timer (Callback)
    // Expire every timer sharing this deadline.
    // Consecutive timers with dt == 0 expire during the same tick.
    while ((scheduler.head != NULL) && (scheduler.head->dt == 0))
    {
        // Unlink, callback, and free one expired timer
        rp22_Timer *expired_timer = scheduler.head;
        scheduler.head = expired_timer->next;

        printf("Tick %u: ", scheduler.tick_count);
        scheduler.on_expired(expired_timer->id);
        free(expired_timer);
        scheduler.freed_count += 1;
    }

    if (scheduler.head != NULL)
    {
        printf("Rem Head dt: %u\n", scheduler.head->dt);
    }
}

static void ReportExpiredTimer(unsigned int timer_id)
{
    printf(">> [Event] Timer %u expired! Action executed.\n", timer_id);
}

static void AssertAllTimersFreed(void)
{
    assert(scheduler.head == NULL);
    assert(scheduler.allocated_count == scheduler.freed_count);
    printf("All timers cleared.\n");
}
