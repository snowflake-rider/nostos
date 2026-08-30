#include "check.h"
#include "remote_event_backlog.h"

#include <stdio.h>

static nostos_job_t make_job(
    uint8_t type,
    uint16_t sequence,
    uint32_t received_ms)
{
    nostos_message_t message = {
        .type = type,
        .source_id = 2U,
        .session_id = 7U,
        .sequence = sequence,
    };
    if (type == NOSTOS_FALL || type == NOSTOS_FALL_CLEAR) {
        message.payload.incident = (nostos_incident_ref_t){7U, 9U};
    }
    nostos_job_t job = {
        .received_ms = received_ms,
        .direction = NOSTOS_TO_UART,
    };
    CHECK(nostos_message_encode(
        &message, job.wire, sizeof(job.wire), &job.length) == NOSTOS_OK);
    return job;
}

static void push(
    remote_event_backlog_t *backlog,
    const nostos_job_t *job,
    nostos_result_t expected)
{
    CHECK(remote_event_backlog_push(
        backlog, job->wire, job->length, job->received_ms) == expected);
}

int main(void)
{
    remote_event_backlog_t backlog;
    remote_event_backlog_init(&backlog);
    nostos_job_t output = {0};
    CHECK(remote_event_backlog_pop(&backlog, 0U, &output) == NOSTOS_EMPTY);

    nostos_job_t up = make_job(NOSTOS_SPEED_UP, 1U, 100U);
    nostos_job_t down = make_job(NOSTOS_SPEED_DOWN, 2U, 101U);
    nostos_job_t stop = make_job(NOSTOS_STOP, 3U, 102U);
    nostos_job_t fall = make_job(NOSTOS_FALL, 4U, 103U);
    nostos_job_t clear = make_job(NOSTOS_FALL_CLEAR, 5U, 104U);
    push(&backlog, &up, NOSTOS_OK);
    push(&backlog, &stop, NOSTOS_OK);
    push(&backlog, &fall, NOSTOS_OK);
    push(&backlog, &down, NOSTOS_OK);
    push(&backlog, &clear, NOSTOS_OK);
    CHECK(remote_event_backlog_pop(&backlog, 105U, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_FALL);
    CHECK(remote_event_backlog_pop(&backlog, 105U, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_FALL_CLEAR);
    CHECK(remote_event_backlog_pop(&backlog, 105U, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_STOP);
    CHECK(remote_event_backlog_pop(&backlog, 105U, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_SPEED_UP);
    CHECK(remote_event_backlog_pop(&backlog, 105U, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_SPEED_DOWN);

    /* An extended STM handshake must never expire a safety incident. */
    fall = make_job(NOSTOS_FALL, 6U, 1U);
    push(&backlog, &fall, NOSTOS_OK);
    CHECK(remote_event_backlog_begin_dispatch(
        &backlog, NOSTOS_BRIDGE_MAX_AGE_MS + 5000U, &output) == NOSTOS_OK);
    CHECK(backlog.dispatch_active);
    CHECK(backlog.count == 1U);
    CHECK(remote_event_backlog_finish_dispatch(&backlog, true) == NOSTOS_OK);
    CHECK(!backlog.dispatch_active);
    CHECK(backlog.urgent_count == 1U);
    CHECK(remote_event_backlog_pop(
        &backlog, NOSTOS_BRIDGE_MAX_AGE_MS + 5000U, &output) == NOSTOS_OK);
    CHECK(output.wire[1] == NOSTOS_FALL);

    up = make_job(NOSTOS_SPEED_UP, 7U, 1U);
    push(&backlog, &up, NOSTOS_OK);
    CHECK(remote_event_backlog_pop(
        &backlog, NOSTOS_BRIDGE_MAX_AGE_MS + 2U, &output) ==
        NOSTOS_EXPIRED);

    /* Normal traffic cannot consume the STOP or urgent safety reserves. */
    remote_event_backlog_init(&backlog);
    for (uint16_t i = 0U; i < REMOTE_EVENT_BACKLOG_NORMAL_CAPACITY; ++i) {
        up = make_job(NOSTOS_SPEED_UP, (uint16_t)(20U + i), 10U);
        push(&backlog, &up, NOSTOS_OK);
    }
    up = make_job(NOSTOS_SPEED_UP, 40U, 10U);
    push(&backlog, &up, NOSTOS_FULL);
    stop = make_job(NOSTOS_STOP, 41U, 10U);
    push(&backlog, &stop, NOSTOS_OK);
    for (uint16_t i = 0U; i < REMOTE_EVENT_BACKLOG_URGENT_RESERVE; ++i) {
        fall = make_job(NOSTOS_FALL, (uint16_t)(50U + i), 10U);
        push(&backlog, &fall, NOSTOS_OK);
    }
    fall = make_job(NOSTOS_FALL, 60U, 10U);
    push(&backlog, &fall, NOSTOS_FULL);
    CHECK(backlog.count == REMOTE_EVENT_BACKLOG_CAPACITY);

    nostos_job_t environment = make_job(NOSTOS_ENVIRONMENT, 70U, 10U);
    push(&backlog, &environment, NOSTOS_UNSUPPORTED_TYPE);

    puts("PASS remote events dispatch FALL/CLEAR > STOP > button FIFO");
    puts("PASS failed UART dispatch restores exact safety wire transactionally");
    puts("PASS STM handshake backlog preserves safety and bounded reserves");
    return 0;
}
