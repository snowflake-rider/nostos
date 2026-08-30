#ifndef NOSTOS_REMOTE_EVENT_BACKLOG_H
#define NOSTOS_REMOTE_EVENT_BACKLOG_H

#include "nostos_bridge.h"

#define REMOTE_EVENT_BACKLOG_CAPACITY 16U
#define REMOTE_EVENT_BACKLOG_URGENT_RESERVE 4U
#define REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY \
    (REMOTE_EVENT_BACKLOG_CAPACITY - REMOTE_EVENT_BACKLOG_URGENT_RESERVE)
#define REMOTE_EVENT_BACKLOG_STOP_RESERVE 1U
#define REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY \
    (REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY - \
     REMOTE_EVENT_BACKLOG_STOP_RESERVE)

typedef struct {
    nostos_job_t jobs[REMOTE_EVENT_BACKLOG_CAPACITY];
    uint8_t urgent_order[REMOTE_EVENT_BACKLOG_CAPACITY];
    uint8_t stop_order[REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY];
    uint8_t normal_order[REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY];
    uint8_t free_slots[REMOTE_EVENT_BACKLOG_CAPACITY];
    size_t urgent_head;
    size_t urgent_count;
    size_t stop_head;
    size_t stop_count;
    size_t normal_head;
    size_t normal_count;
    size_t free_count;
    size_t count;
    uint8_t dispatch_slot;
    uint8_t dispatch_class;
    bool dispatch_active;
} remote_event_backlog_t;

void remote_event_backlog_init(remote_event_backlog_t *backlog);

/* Stores only paired-display event traffic. Four slots stay reserved for
 * FALL/CLEAR and one nonurgent slot stays reserved for STOP. */
nostos_result_t remote_event_backlog_push(
    remote_event_backlog_t *backlog,
    const uint8_t *wire,
    size_t length,
    uint32_t received_ms);

/* Dispatch is strict FALL/CLEAR > STOP > buttons. FALL/CLEAR survives an
 * arbitrarily long STM handshake; lower classes retain the normal 2 s TTL. */
nostos_result_t remote_event_backlog_pop(
    remote_event_backlog_t *backlog,
    uint32_t now_ms,
    nostos_job_t *job);

/* Transactional worker dispatch keeps the selected slot inside the bounded
 * capacity until UART success is known. `retry=true` restores the exact wire
 * to its original priority class without opening a producer race. */
nostos_result_t remote_event_backlog_begin_dispatch(
    remote_event_backlog_t *backlog,
    uint32_t now_ms,
    nostos_job_t *job);
nostos_result_t remote_event_backlog_finish_dispatch(
    remote_event_backlog_t *backlog,
    bool retry);

#endif
