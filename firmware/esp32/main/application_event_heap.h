#ifndef NOSTOS_APPLICATION_EVENT_HEAP_H
#define NOSTOS_APPLICATION_EVENT_HEAP_H

#include "nostos_bridge.h"

#define APPLICATION_EVENT_HEAP_CAPACITY 16U
#define APPLICATION_EVENT_HEAP_URGENT_RESERVE 4U
#define APPLICATION_EVENT_HEAP_STOP_RESERVE 1U
#define APPLICATION_EVENT_HEAP_NORMAL_CAPACITY \
    (APPLICATION_EVENT_HEAP_CAPACITY - APPLICATION_EVENT_HEAP_URGENT_RESERVE - \
     APPLICATION_EVENT_HEAP_STOP_RESERVE)
#define APPLICATION_EVENT_HEAP_NONURGENT_CAPACITY \
    (APPLICATION_EVENT_HEAP_CAPACITY - APPLICATION_EVENT_HEAP_URGENT_RESERVE)

typedef struct {
    nostos_job_t job;
    uint64_t arrival_order;
    uint8_t priority;
} application_event_heap_entry_t;

typedef struct {
    application_event_heap_entry_t entries[APPLICATION_EVENT_HEAP_CAPACITY];
    size_t count;
    size_t urgent_count;
    size_t stop_count;
    size_t normal_count;
    uint64_t next_arrival_order;
} application_event_heap_t;

void application_event_heap_init(application_event_heap_t *heap);

/* Fixed min-heap key: P1 FALL/CLEAR, P2 STOP, P3 SPEED_DOWN, P4 SPEED_UP;
 * equal priority is stable FIFO. Normal traffic cannot consume the four
 * urgent slots or the one STOP slot. */
nostos_result_t application_event_heap_push(
    application_event_heap_t *heap,
    const uint8_t *wire,
    size_t length,
    uint32_t received_ms);

nostos_result_t application_event_heap_pop(
    application_event_heap_t *heap,
    uint32_t now_ms,
    nostos_job_t *job);

/* Returns 1..4 for the current minimum or zero when empty. */
uint8_t application_event_heap_priority(
    const application_event_heap_t *heap);

/* Trusted remote-session replacement drops queued output from older sessions
 * of that source before the new packet can be scheduled. */
size_t application_event_heap_discard_source_before_session(
    application_event_heap_t *heap,
    uint8_t source_id,
    uint32_t session_id);

#endif /* NOSTOS_APPLICATION_EVENT_HEAP_H */
