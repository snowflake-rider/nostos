#include "../case_helpers.h"

#include "event_bridge.h"

#include <stdint.h>
#include <string.h>

static void v1_validation_and_age(void)
{
    event_bridge_t bridge;
    event_job_t job;
    const uint8_t fall_wire[EVENT_WIRE_SIZE] = {EVENT_WIRE_VERSION, MSG_FALL_DETECTED};

    event_bridge_init(&bridge);
    CASE_CHECK(event_bridge_uart(&bridge, MSG_NONE, 0U, true) == EVENT_NOOP);
    CASE_CHECK(event_bridge_uart(&bridge, MSG_UNKNOWN, 0U, true) == EVENT_INVALID);
    CASE_CHECK(event_bridge_uart(&bridge, MSG_FALL_DETECTED, 0U, false) == EVENT_NOT_READY);
    CASE_CHECK(event_bridge_uart(&bridge, MSG_FALL_DETECTED, 0U, true) == EVENT_OK);
    CASE_CHECK(event_bridge_next(&bridge, EVENT_MAX_AGE_MS - 1U, true, &job) == EVENT_OK);
    CASE_CHECK(job.id == MSG_FALL_DETECTED && job.direction == EVENT_TO_MESH);

    CASE_CHECK(event_bridge_uart(&bridge, MSG_FALL_DETECTED, 0U, true) == EVENT_OK);
    CASE_CHECK(event_bridge_next(&bridge, EVENT_MAX_AGE_MS, true, &job) == EVENT_EXPIRED);
    CASE_CHECK(event_bridge_mesh(&bridge, fall_wire, sizeof(fall_wire), 0x0101U,
                                 0x0101U, 0U) == EVENT_SELF);
    CASE_CHECK(event_bridge_mesh(&bridge, fall_wire, sizeof(fall_wire), 0U,
                                 0x0202U, 0U) == EVENT_INVALID);
    puts("SINGLE 1/5 v1 validation + age boundary PASS");
}

static void v1_capacity(void)
{
    event_bridge_t bridge;
    event_job_t job;
    event_bridge_init(&bridge);
    for (size_t index = 0U; index < EVENT_QUEUE_CAPACITY; ++index) {
        CASE_CHECK(event_bridge_uart(&bridge, MSG_STOP_REQUEST, 0U, true) == EVENT_OK);
    }
    CASE_CHECK(event_bridge_uart(&bridge, MSG_FALL_DETECTED, 0U, true) == EVENT_FULL);
    CASE_CHECK(bridge.count == EVENT_QUEUE_CAPACITY);
    for (size_t index = 0U; index < EVENT_QUEUE_CAPACITY; ++index) {
        CASE_CHECK(event_bridge_next(&bridge, 0U, true, &job) == EVENT_OK);
        CASE_CHECK(job.id == MSG_STOP_REQUEST);
    }
    CASE_CHECK(event_bridge_next(&bridge, 0U, true, &job) == EVENT_EMPTY);
    puts("SINGLE 2/5 v1 FIFO capacity PASS");
}

static void v2_reserve_and_owned_copy(void)
{
    nostos_bridge_t bridge;
    nostos_job_t job;
    uint8_t wire[NOSTOS_WIRE_MAX];
    nostos_peer_t invalid_peers[NOSTOS_NODE_COUNT];
    memcpy(invalid_peers, case_peers, sizeof(invalid_peers));
    invalid_peers[2].source_id = invalid_peers[1].source_id;
    CASE_CHECK(nostos_bridge_init(&bridge, 2U, invalid_peers) == NOSTOS_BAD_VALUE);
    CASE_CHECK(nostos_bridge_init(&bridge, 2U, case_peers) == NOSTOS_OK);

    size_t length = case_encode(case_message(NOSTOS_STOP, 2U, 1U, 1U), wire);
    for (size_t index = 0U; index < NOSTOS_BRIDGE_NORMAL_CAPACITY; ++index) {
        CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                        0U, 0U, true) == NOSTOS_OK);
    }
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 0U, true) == NOSTOS_FULL);

    length = case_encode(case_message(NOSTOS_FALL, 2U, 2U, 7U), wire);
    for (size_t index = 0U; index < NOSTOS_BRIDGE_URGENT_RESERVE; ++index) {
        CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                        0U, 0U, true) == NOSTOS_OK);
    }
    memset(wire, 0, sizeof(wire));
    CASE_CHECK(nostos_bridge_next(&bridge, 0U, true, &job) == NOSTOS_OK);
    CASE_CHECK(job.wire[1] == NOSTOS_FALL && job.wire[2] == 2U);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, job.wire, job.length,
                                    0U, 0U, true) == NOSTOS_OK);
    puts("SINGLE 3/5 v2 urgent reserve + owned payload PASS");
}

static void v2_source_and_age(void)
{
    nostos_bridge_t bridge;
    nostos_job_t job;
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = case_encode(case_message(NOSTOS_FALL, 2U, 1U, 1U), wire);

    CASE_CHECK(nostos_bridge_init(&bridge, 3U, case_peers) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length,
                                    0x0101U, 0U, true) == NOSTOS_UNAUTHORIZED);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length,
                                    0x0202U, 0U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&bridge, NOSTOS_BRIDGE_MAX_AGE_MS, false, &job) == NOSTOS_OK);
    CASE_CHECK(job.direction == NOSTOS_TO_UART);

    CASE_CHECK(nostos_bridge_init(&bridge, 2U, case_peers) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 0U, false) == NOSTOS_NOT_READY);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 0U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&bridge, NOSTOS_BRIDGE_MAX_AGE_MS + 1U,
                                  true, &job) == NOSTOS_EXPIRED);
    puts("SINGLE 4/5 v2 source binding + age boundary PASS");
}

static void uart_frame_recovery(void)
{
    uint8_t wire[NOSTOS_WIRE_MAX];
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    uint8_t output[NOSTOS_WIRE_MAX];
    size_t wire_length = case_encode(case_message(NOSTOS_FALL, 2U, 4U, 9U), wire);
    size_t frame_length = 0U;
    size_t output_length = 0U;
    nostos_uart_parser_t parser = {0};

    CASE_CHECK(nostos_uart_encode(wire, wire_length, frame, sizeof(frame),
                                  &frame_length) == NOSTOS_OK);
    for (size_t index = 0U; index + 1U < frame_length; ++index) {
        CASE_CHECK(nostos_uart_feed(&parser, frame[index], 0U, output,
                                    &output_length) == NOSTOS_EMPTY);
    }
    CASE_CHECK(nostos_uart_feed(&parser, frame[frame_length - 1U], 0U, output,
                                &output_length) == NOSTOS_OK);
    CASE_CHECK(output_length == wire_length && memcmp(output, wire, wire_length) == 0);

    frame[2] ^= 1U;
    nostos_uart_reset(&parser);
    nostos_result_t result = NOSTOS_EMPTY;
    for (size_t index = 0U; index < frame_length; ++index) {
        result = nostos_uart_feed(&parser, frame[index], 1U, output, &output_length);
    }
    CASE_CHECK(result == NOSTOS_BAD_CRC);
    puts("SINGLE 5/5 UART split frame + CRC rejection PASS");
}

int main(void)
{
    v1_validation_and_age();
    v1_capacity();
    v2_reserve_and_owned_copy();
    v2_source_and_age();
    uart_frame_recovery();
    puts("SINGLE_CASES=PASS count=5; REAL_BLE_RF=NOT_TESTED");
    return 0;
}
