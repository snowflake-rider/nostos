#include "../case_helpers.h"

#include "event_bridge.h"

#include <stdint.h>
#include <string.h>

static void fall_during_normal_saturation(void)
{
    event_bridge_t legacy;
    event_bridge_init(&legacy);
    for (size_t index = 0U; index < EVENT_QUEUE_CAPACITY; ++index) {
        CASE_CHECK(event_bridge_uart(&legacy, MSG_STOP_REQUEST, 0U, true) == EVENT_OK);
    }
    CASE_CHECK(event_bridge_uart(&legacy, MSG_FALL_DETECTED, 0U, true) == EVENT_FULL);

    nostos_bridge_t bridge;
    nostos_job_t job;
    uint8_t wire[NOSTOS_WIRE_MAX];
    CASE_CHECK(nostos_bridge_init(&bridge, 2U, case_peers) == NOSTOS_OK);
    size_t length = case_encode(case_message(NOSTOS_STOP, 2U, 1U, 1U), wire);
    for (size_t index = 0U; index < NOSTOS_BRIDGE_NORMAL_CAPACITY; ++index) {
        CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                        0U, 0U, true) == NOSTOS_OK);
    }
    length = case_encode(case_message(NOSTOS_FALL, 2U, 2U, 10U), wire);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 0U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&bridge, 0U, true, &job) == NOSTOS_OK);
    CASE_CHECK(job.wire[1] == NOSTOS_FALL);
    puts("COMPLEX 1/5 normal saturation + FALL reserve PASS (v1 limitation confirmed)");
}

static void urgent_flood_fairness(void)
{
    static const uint8_t urgent_types[5] = {
        NOSTOS_FALL, NOSTOS_SOS, NOSTOS_FALL_CLEAR, NOSTOS_SOS_CLEAR, NOSTOS_FALL
    };
    nostos_bridge_t bridge;
    nostos_job_t job;
    uint8_t wire[NOSTOS_WIRE_MAX];
    CASE_CHECK(nostos_bridge_init(&bridge, 2U, case_peers) == NOSTOS_OK);

    nostos_message_t normal = case_message(NOSTOS_ENVIRONMENT, 2U, 10U, 1U);
    normal.payload.environment = (nostos_environment_t){
        350, 500U, NOSTOS_VALID, NOSTOS_VALID
    };
    size_t length = case_encode(normal, wire);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 0U, true) == NOSTOS_OK);
    for (size_t index = 0U; index < 5U; ++index) {
        length = case_encode(case_message(urgent_types[index], 2U,
                                          (uint16_t)(11U + index),
                                          (uint16_t)(20U + index)), wire);
        CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                        0U, 0U, true) == NOSTOS_OK);
    }
    for (size_t index = 0U; index < NOSTOS_BRIDGE_URGENT_BURST; ++index) {
        CASE_CHECK(nostos_bridge_next(&bridge, 0U, true, &job) == NOSTOS_OK);
        CASE_CHECK(job.wire[1] == urgent_types[index]);
    }
    CASE_CHECK(nostos_bridge_next(&bridge, 0U, true, &job) == NOSTOS_OK);
    CASE_CHECK(job.wire[1] == NOSTOS_ENVIRONMENT);
    CASE_CHECK(nostos_bridge_next(&bridge, 0U, true, &job) == NOSTOS_OK);
    CASE_CHECK(job.wire[1] == NOSTOS_FALL);
    puts("COMPLEX 2/5 urgent flood + 4:1 starvation guard PASS");
}

static void fall_clear_duplicate_and_reorder(void)
{
    nostos_receiver_t receiver = case_receiver(3U);
    nostos_message_t fall = case_message(NOSTOS_FALL, 2U, 10U, 30U);
    CASE_CHECK(nostos_receiver_apply(&receiver, &fall, 100U) == NOSTOS_OK);
    CASE_CHECK(nostos_receiver_outputs(&receiver, 100U).buzzer == NOSTOS_BUZZER_EMERGENCY);
    CASE_CHECK(nostos_receiver_apply(&receiver, &fall, 101U) == NOSTOS_DUPLICATE);

    nostos_message_t clear = case_message(NOSTOS_FALL_CLEAR, 2U, 11U, 30U);
    CASE_CHECK(nostos_receiver_apply(&receiver, &clear, 102U) == NOSTOS_OK);
    CASE_CHECK(nostos_receiver_outputs(&receiver, 102U).led == NOSTOS_LED_OFF);
    fall.sequence = 12U;
    CASE_CHECK(nostos_receiver_apply(&receiver, &fall, 103U) == NOSTOS_STALE);

    nostos_message_t reordered = case_message(NOSTOS_HEARTBEAT, 2U, 9U, 1U);
    reordered.payload.status = 0U;
    CASE_CHECK(nostos_receiver_apply(&receiver, &reordered, 104U) == NOSTOS_OK);
    CASE_CHECK(nostos_receiver_apply(&receiver, &reordered, 105U) == NOSTOS_DUPLICATE);

    nostos_message_t next_fall = case_message(NOSTOS_FALL, 2U, 13U, 31U);
    CASE_CHECK(nostos_receiver_apply(&receiver, &next_fall, 106U) == NOSTOS_OK);
    CASE_CHECK(nostos_receiver_outputs(&receiver, 106U).led == NOSTOS_LED_RED_BLINK);
    puts("COMPLEX 3/5 FALL/CLEAR + duplicate + out-of-order window PASS");
}

static void two_direction_delivery_without_republish(void)
{
    nostos_bridge_t sender_bridge;
    nostos_bridge_t receiver_bridge;
    nostos_job_t air;
    nostos_job_t uart_job;
    nostos_receiver_t receiver = case_receiver(3U);
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = case_encode(case_message(NOSTOS_FALL, 2U, 20U, 40U), wire);

    CASE_CHECK(nostos_bridge_init(&sender_bridge, 2U, case_peers) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_init(&receiver_bridge, 3U, case_peers) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_accept(&sender_bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 0U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&sender_bridge, 1U, true, &air) == NOSTOS_OK);
    CASE_CHECK(air.direction == NOSTOS_TO_MESH);

    CASE_CHECK(nostos_bridge_accept(&receiver_bridge, NOSTOS_TO_UART,
                                    air.wire, air.length, 0x0202U, 2U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&receiver_bridge, 3U, true, &uart_job) == NOSTOS_OK);
    CASE_CHECK(uart_job.direction == NOSTOS_TO_UART);
    CASE_CHECK(nostos_receiver_wire(&receiver, uart_job.wire, uart_job.length, 4U) == NOSTOS_OK);
    CASE_CHECK(nostos_receiver_wire(&receiver, uart_job.wire, uart_job.length, 5U) == NOSTOS_DUPLICATE);
    CASE_CHECK(nostos_receiver_outputs(&receiver, 5U).buzzer == NOSTOS_BUZZER_EMERGENCY);
    CASE_CHECK(nostos_bridge_next(&receiver_bridge, 5U, true, &uart_job) == NOSTOS_EMPTY);
    puts("COMPLEX 4/5 UART -> Mesh -> UART + duplicate once + no republish PASS");
}

static void mixed_failures_and_wraparound(void)
{
    nostos_bridge_t bridge;
    nostos_job_t job;
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = case_encode(case_message(NOSTOS_FALL, 2U, 1U, 50U), wire);
    CASE_CHECK(nostos_bridge_init(&bridge, 2U, case_peers) == NOSTOS_OK);

    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, UINT32_MAX - 100U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&bridge, 50U, true, &job) == NOSTOS_OK);

    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
                                    0U, 100U, true) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_next(&bridge, 101U, false, &job) == NOSTOS_NOT_READY);
    CASE_CHECK(nostos_bridge_next(&bridge, 101U, true, &job) == NOSTOS_EMPTY);

    CASE_CHECK(nostos_bridge_init(&bridge, 3U, case_peers) == NOSTOS_OK);
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length,
                                    0x0101U, 0U, true) == NOSTOS_UNAUTHORIZED);
    CASE_CHECK(bridge.count == 0U);
    wire[0] = 1U;
    CASE_CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length,
                                    0x0202U, 0U, true) == NOSTOS_UNSUPPORTED_VERSION);
    CASE_CHECK(bridge.count == 0U);
    puts("COMPLEX 5/5 wraparound + readiness loss + invalid source/version PASS");
}

int main(void)
{
    fall_during_normal_saturation();
    urgent_flood_fairness();
    fall_clear_duplicate_and_reorder();
    two_direction_delivery_without_republish();
    mixed_failures_and_wraparound();
    puts("COMPLEX_CASES=PASS count=5; REAL_BLE_RF=NOT_TESTED");
    return 0;
}
