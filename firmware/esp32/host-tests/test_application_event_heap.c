#include "application_event_heap.h"
#include "check.h"

#include <stdio.h>

static void push_event(
    application_event_heap_t *heap,
    uint8_t type,
    uint16_t sequence,
    uint32_t now_ms,
    nostos_result_t expected)
{
    nostos_message_t message = {
        .type = type,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = sequence,
    };
    if (type == NOSTOS_FALL || type == NOSTOS_FALL_CLEAR) {
        message.payload.incident = (nostos_incident_ref_t){
            .session_id = 10U,
            .incident_id = (uint16_t)(sequence + 1U),
        };
    }
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    CHECK(nostos_message_encode(
        &message, wire, sizeof(wire), &length) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        heap, wire, length, now_ms) == expected);
}

static void push_event_from_session(
    application_event_heap_t *heap,
    uint8_t source_id,
    uint32_t session_id,
    uint16_t sequence)
{
    nostos_message_t message = {
        .type = NOSTOS_SPEED_UP,
        .source_id = source_id,
        .session_id = session_id,
        .sequence = sequence,
    };
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    CHECK(nostos_message_encode(
        &message, wire, sizeof(wire), &length) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        heap, wire, length, 0U) == NOSTOS_OK);
}

static uint8_t pop_type(application_event_heap_t *heap, uint32_t now_ms)
{
    nostos_job_t job;
    CHECK(application_event_heap_pop(heap, now_ms, &job) == NOSTOS_OK);
    nostos_message_t message;
    CHECK(nostos_message_decode(
        job.wire, job.length, &message) == NOSTOS_OK);
    return message.type;
}

int main(void)
{
    application_event_heap_t heap;
    application_event_heap_init(&heap);
    push_event(&heap, NOSTOS_SPEED_UP, 0U, 0U, NOSTOS_OK);
    push_event(&heap, NOSTOS_SPEED_DOWN, 1U, 1U, NOSTOS_OK);
    push_event(&heap, NOSTOS_SPEED_DOWN, 2U, 2U, NOSTOS_OK);
    push_event(&heap, NOSTOS_STOP, 3U, 3U, NOSTOS_OK);
    push_event(&heap, NOSTOS_FALL, 4U, 4U, NOSTOS_OK);
    CHECK(application_event_heap_priority(&heap) == 1U);
    CHECK(pop_type(&heap, 5U) == NOSTOS_FALL);
    CHECK(pop_type(&heap, 5U) == NOSTOS_STOP);
    CHECK(pop_type(&heap, 5U) == NOSTOS_SPEED_DOWN);
    CHECK(pop_type(&heap, 5U) == NOSTOS_SPEED_DOWN);
    CHECK(pop_type(&heap, 5U) == NOSTOS_SPEED_UP);

    application_event_heap_init(&heap);
    for (uint16_t i = 0U; i < APPLICATION_EVENT_HEAP_NORMAL_CAPACITY; ++i) {
        push_event(&heap, NOSTOS_SPEED_UP, i, 0U, NOSTOS_OK);
    }
    push_event(&heap, NOSTOS_SPEED_DOWN, 20U, 0U, NOSTOS_FULL);
    push_event(&heap, NOSTOS_STOP, 21U, 0U, NOSTOS_OK);
    for (uint16_t i = 0U; i < APPLICATION_EVENT_HEAP_URGENT_RESERVE; ++i) {
        push_event(&heap, NOSTOS_FALL, (uint16_t)(30U + i), 0U, NOSTOS_OK);
    }
    CHECK(heap.count == APPLICATION_EVENT_HEAP_CAPACITY);

    application_event_heap_init(&heap);
    push_event_from_session(&heap, 2U, 10U, 0U);
    push_event_from_session(&heap, 1U, 10U, 1U);
    push_event_from_session(&heap, 2U, 11U, 2U);
    CHECK(application_event_heap_discard_source_before_session(
        &heap, 2U, 11U) == 1U);
    CHECK(heap.count == 2U);
    nostos_job_t job;
    nostos_message_t message;
    CHECK(application_event_heap_pop(&heap, 0U, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(job.wire, job.length, &message) == NOSTOS_OK);
    CHECK(message.source_id == 1U && message.session_id == 10U);
    CHECK(application_event_heap_pop(&heap, 0U, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(job.wire, job.length, &message) == NOSTOS_OK);
    CHECK(message.source_id == 2U && message.session_id == 11U);
    puts("PASS fixed min-heap order P1/P2/P3/P4 with stable FIFO");
    puts("PASS normal flood preserves one STOP and four urgent slots");
    puts("PASS new remote session discards queued output from old session");
    return 0;
}
