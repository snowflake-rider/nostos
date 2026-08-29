/* 김현수, Redpill Day 18. See README.md for contracts and adaptations. */
#ifndef REDPILL_MIN_HEAP_H
#define REDPILL_MIN_HEAP_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

typedef struct rp18_Task {
  uint8_t task_id;
  uint8_t priority;
} rp18_Task;

typedef struct rp18_TaskScheduler {
  rp18_Task *tasks;
  size_t capacity;
  size_t task_count;
} rp18_TaskScheduler;

bool rp18_init_task_scheduler(rp18_TaskScheduler *ts, const size_t capacity);

/* Output must not overlap the scheduler's internal tasks array. */
bool rp18_extract(rp18_TaskScheduler *ts, rp18_Task *task);

/* Copies the task before growing storage; new_task may be an existing item.
 * Any pointers into tasks must be reacquired after a successful insertion. */
bool rp18_insert(rp18_TaskScheduler *ts, const rp18_Task *new_task);

int rp18_demo(void);

#endif
