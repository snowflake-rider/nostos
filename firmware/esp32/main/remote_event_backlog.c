#include "remote_event_backlog.h"

#include <string.h>

static bool urgent_type(uint8_t type)
{
    return type == NOSTOS_FALL || type == NOSTOS_FALL_CLEAR;
}

static bool stop_type(uint8_t type)
{
    return type == NOSTOS_STOP;
}

static bool button_type(uint8_t type)
{
    return type == NOSTOS_SPEED_UP || type == NOSTOS_SPEED_DOWN;
}

enum {
    DISPATCH_NORMAL = 1U,
    DISPATCH_STOP = 2U,
    DISPATCH_URGENT = 3U,
};

void remote_event_backlog_init(remote_event_backlog_t *backlog)
{
    if (backlog == NULL) return;
    *backlog = (remote_event_backlog_t){
        .free_count = REMOTE_EVENT_BACKLOG_CAPACITY,
    };
    for (size_t i = 0U; i < REMOTE_EVENT_BACKLOG_CAPACITY; ++i) {
        backlog->free_slots[i] =
            (uint8_t)(REMOTE_EVENT_BACKLOG_CAPACITY - 1U - i);
    }
}

nostos_result_t remote_event_backlog_push(
    remote_event_backlog_t *backlog,
    const uint8_t *wire,
    size_t length,
    uint32_t received_ms)
{
    if (backlog == NULL || wire == NULL) return NOSTOS_BAD_ARGUMENT;
    nostos_message_t message;
    nostos_result_t decoded = nostos_message_decode(wire, length, &message);
    if (decoded != NOSTOS_OK) return decoded;
    bool urgent = urgent_type(message.type);
    bool stop = stop_type(message.type);
    if (!urgent && !stop && !button_type(message.type)) {
        return NOSTOS_UNSUPPORTED_TYPE;
    }

    bool dispatch_nonurgent = backlog->dispatch_active &&
        backlog->dispatch_class != DISPATCH_URGENT;
    bool dispatch_normal = backlog->dispatch_active &&
        backlog->dispatch_class == DISPATCH_NORMAL;
    size_t nonurgent_count = backlog->stop_count + backlog->normal_count +
        (dispatch_nonurgent ? 1U : 0U);
    size_t normal_count = backlog->normal_count +
        (dispatch_normal ? 1U : 0U);
    if (backlog->count == REMOTE_EVENT_BACKLOG_CAPACITY ||
        (!urgent &&
         nonurgent_count == REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY) ||
        (!urgent && !stop &&
            normal_count == REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY)) {
        return NOSTOS_FULL;
    }

    uint8_t slot = backlog->free_slots[--backlog->free_count];
    nostos_job_t *job = &backlog->jobs[slot];
    *job = (nostos_job_t){
        .length = length,
        .received_ms = received_ms,
        .direction = NOSTOS_TO_UART,
    };
    memcpy(job->wire, wire, length);
    if (urgent) {
        size_t tail = (backlog->urgent_head + backlog->urgent_count) %
            REMOTE_EVENT_BACKLOG_CAPACITY;
        backlog->urgent_order[tail] = slot;
        ++backlog->urgent_count;
    } else if (stop) {
        size_t tail = (backlog->stop_head + backlog->stop_count) %
            REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY;
        backlog->stop_order[tail] = slot;
        ++backlog->stop_count;
    } else {
        size_t tail = (backlog->normal_head + backlog->normal_count) %
            REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY;
        backlog->normal_order[tail] = slot;
        ++backlog->normal_count;
    }
    ++backlog->count;
    return NOSTOS_OK;
}

nostos_result_t remote_event_backlog_pop(
    remote_event_backlog_t *backlog,
    uint32_t now_ms,
    nostos_job_t *job)
{
    nostos_result_t result = remote_event_backlog_begin_dispatch(
        backlog, now_ms, job);
    if (result == NOSTOS_OK) {
        nostos_result_t finished = remote_event_backlog_finish_dispatch(
            backlog, false);
        return finished == NOSTOS_OK ? NOSTOS_OK : finished;
    }
    return result;
}

nostos_result_t remote_event_backlog_begin_dispatch(
    remote_event_backlog_t *backlog,
    uint32_t now_ms,
    nostos_job_t *job)
{
    if (backlog == NULL || job == NULL) return NOSTOS_BAD_ARGUMENT;
    if (backlog->dispatch_active) return NOSTOS_CONFLICT;
    if (backlog->count == 0U) return NOSTOS_EMPTY;

    uint8_t slot;
    bool urgent;
    if (backlog->urgent_count != 0U) {
        urgent = true;
        backlog->dispatch_class = DISPATCH_URGENT;
        slot = backlog->urgent_order[backlog->urgent_head];
        backlog->urgent_head = (backlog->urgent_head + 1U) %
            REMOTE_EVENT_BACKLOG_CAPACITY;
        --backlog->urgent_count;
    } else if (backlog->stop_count != 0U) {
        urgent = false;
        backlog->dispatch_class = DISPATCH_STOP;
        slot = backlog->stop_order[backlog->stop_head];
        backlog->stop_head = (backlog->stop_head + 1U) %
            REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY;
        --backlog->stop_count;
    } else {
        urgent = false;
        backlog->dispatch_class = DISPATCH_NORMAL;
        slot = backlog->normal_order[backlog->normal_head];
        backlog->normal_head = (backlog->normal_head + 1U) %
            REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY;
        --backlog->normal_count;
    }
    *job = backlog->jobs[slot];
    if (!urgent &&
        (uint32_t)(now_ms - job->received_ms) > NOSTOS_BRIDGE_MAX_AGE_MS) {
        backlog->free_slots[backlog->free_count++] = slot;
        --backlog->count;
        backlog->dispatch_class = 0U;
        return NOSTOS_EXPIRED;
    }
    backlog->dispatch_slot = slot;
    backlog->dispatch_active = true;
    return NOSTOS_OK;
}

nostos_result_t remote_event_backlog_finish_dispatch(
    remote_event_backlog_t *backlog,
    bool retry)
{
    if (backlog == NULL) return NOSTOS_BAD_ARGUMENT;
    if (!backlog->dispatch_active) return NOSTOS_EMPTY;
    uint8_t slot = backlog->dispatch_slot;
    if (retry) {
        if (backlog->dispatch_class == DISPATCH_URGENT) {
            size_t tail = (backlog->urgent_head + backlog->urgent_count) %
                REMOTE_EVENT_BACKLOG_CAPACITY;
            backlog->urgent_order[tail] = slot;
            ++backlog->urgent_count;
        } else if (backlog->dispatch_class == DISPATCH_STOP) {
            size_t tail = (backlog->stop_head + backlog->stop_count) %
                REMOTE_EVENT_BACKLOG_NONURGENT_CAPACITY;
            backlog->stop_order[tail] = slot;
            ++backlog->stop_count;
        } else if (backlog->dispatch_class == DISPATCH_NORMAL) {
            size_t tail = (backlog->normal_head + backlog->normal_count) %
                REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY;
            backlog->normal_order[tail] = slot;
            ++backlog->normal_count;
        } else {
            return NOSTOS_CONFLICT;
        }
    } else {
        backlog->free_slots[backlog->free_count++] = slot;
        --backlog->count;
    }
    backlog->dispatch_slot = 0U;
    backlog->dispatch_class = 0U;
    backlog->dispatch_active = false;
    return NOSTOS_OK;
}
