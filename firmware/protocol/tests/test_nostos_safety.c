#include "check.h"
#include "nostos_bridge.h"
#include "nostos_endpoint.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static size_t encode_message(uint8_t type, uint8_t source, uint32_t session,
                             uint16_t sequence, uint8_t wire[NOSTOS_WIRE_MAX])
{
    nostos_message_t message = {
        .type = type,
        .source_id = source,
        .session_id = session,
        .sequence = sequence,
    };
    if ((type == NOSTOS_FALL) || (type == NOSTOS_FALL_CLEAR))
    {
        message.payload.incident = (nostos_incident_ref_t){session, sequence};
    }
    size_t length = 0U;
    CHECK(nostos_message_encode(&message, wire, NOSTOS_WIRE_MAX, &length) == NOSTOS_OK);
    return length;
}

static size_t encode_sensor_message(uint8_t type, uint8_t source,
                                    uint8_t wire[NOSTOS_WIRE_MAX])
{
    nostos_message_t message = {
        .type = type,
        .source_id = source,
        .session_id = 1U,
        .sequence = 1U,
    };
    if (type == NOSTOS_RIDE) {
        message.payload.ride = (nostos_ride_t){true, 120U, 1000U};
    } else {
        CHECK(type == NOSTOS_ENVIRONMENT);
        message.payload.environment = (nostos_environment_t){
            .temperature_c_x10 = 250,
            .humidity_pct_x10 = 600U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        };
    }
    size_t length = 0U;
    CHECK(nostos_message_encode(&message, wire, NOSTOS_WIRE_MAX, &length) ==
        NOSTOS_OK);
    return length;
}

static nostos_result_t feed_frame(const uint8_t *frame, size_t length,
                                  uint8_t wire[NOSTOS_WIRE_MAX], size_t *wire_length)
{
    nostos_uart_parser_t parser = {0};
    nostos_result_t result = NOSTOS_EMPTY;
    for (size_t index = 0U; index < length; ++index)
    {
        result = nostos_uart_feed(&parser, frame[index], (uint32_t)index,
                                  wire, wire_length);
    }
    return result;
}

static void crc_and_frame_validation(void)
{
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t wire_length = encode_message(NOSTOS_STOP, 2U, 77U, 4U, wire);
    uint8_t frame[NOSTOS_UART_FRAME_MAX];
    size_t frame_length = 0U;
    CHECK(nostos_uart_encode(wire, wire_length, frame, sizeof(frame), &frame_length) == NOSTOS_OK);

    uint8_t decoded[NOSTOS_WIRE_MAX] = {0};
    size_t decoded_length = 0U;
    CHECK(feed_frame(frame, frame_length, decoded, &decoded_length) == NOSTOS_OK);
    CHECK(decoded_length == wire_length);
    CHECK(memcmp(decoded, wire, wire_length) == 0);

    frame[2] ^= 0x01U;
    decoded_length = 0U;
    CHECK(feed_frame(frame, frame_length, decoded, &decoded_length) == NOSTOS_BAD_CRC);
}

static void session_and_sequence_replay_protection(void)
{
    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);

    nostos_message_t request = {
        .type = NOSTOS_STOP,
        .source_id = 2U,
        .session_id = 77U,
        .sequence = 10U,
    };
    CHECK(nostos_receiver_apply(&receiver, &request, 100U) == NOSTOS_SESSION_REQUIRED);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 77U, 10U) == NOSTOS_OK);
    CHECK(nostos_receiver_apply(&receiver, &request, 101U) == NOSTOS_OK);
    CHECK(nostos_receiver_apply(&receiver, &request, 102U) == NOSTOS_DUPLICATE);

    request.sequence = 9U;
    CHECK(nostos_receiver_apply(&receiver, &request, 103U) == NOSTOS_STALE);
    CHECK(receiver.pending_stop.pending);
    CHECK(receiver.pending_stop.message.type == NOSTOS_STOP);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 77U, 0U) == NOSTOS_STALE);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 78U, 0U) == NOSTOS_OK);
    CHECK(!receiver.pending_stop.pending);
}

static void check_report_equal(
    const nostos_report_t *actual,
    const nostos_report_t *expected)
{
    CHECK(actual->session_id == expected->session_id);
    CHECK(actual->sequence == expected->sequence);
    CHECK(actual->received_ms == expected->received_ms);
    CHECK(actual->seen == expected->seen);
}

static void check_node_equal(
    const nostos_node_state_t *actual,
    const nostos_node_state_t *expected)
{
    CHECK(actual->source_id == expected->source_id);
    check_report_equal(&actual->environment.report, &expected->environment.report);
    CHECK(actual->environment.temperature_c_x10.value ==
        expected->environment.temperature_c_x10.value);
    CHECK(actual->environment.temperature_c_x10.quality ==
        expected->environment.temperature_c_x10.quality);
    CHECK(actual->environment.temperature_c_x10.has_value ==
        expected->environment.temperature_c_x10.has_value);
    CHECK(actual->environment.temperature_c_x10.value_received_ms ==
        expected->environment.temperature_c_x10.value_received_ms);
    CHECK(actual->environment.humidity_pct_x10.value ==
        expected->environment.humidity_pct_x10.value);
    CHECK(actual->environment.humidity_pct_x10.quality ==
        expected->environment.humidity_pct_x10.quality);
    CHECK(actual->environment.humidity_pct_x10.has_value ==
        expected->environment.humidity_pct_x10.has_value);
    CHECK(actual->environment.humidity_pct_x10.value_received_ms ==
        expected->environment.humidity_pct_x10.value_received_ms);
    check_report_equal(&actual->ride.report, &expected->ride.report);
    CHECK(actual->ride.speed_kmh_x10.value == expected->ride.speed_kmh_x10.value);
    CHECK(actual->ride.speed_kmh_x10.quality == expected->ride.speed_kmh_x10.quality);
    CHECK(actual->ride.speed_kmh_x10.has_value == expected->ride.speed_kmh_x10.has_value);
    CHECK(actual->ride.speed_kmh_x10.value_received_ms ==
        expected->ride.speed_kmh_x10.value_received_ms);
    CHECK(actual->ride.distance_mm.value == expected->ride.distance_mm.value);
    CHECK(actual->ride.distance_mm.quality == expected->ride.distance_mm.quality);
    CHECK(actual->ride.distance_mm.has_value == expected->ride.distance_mm.has_value);
    CHECK(actual->ride.distance_mm.value_received_ms ==
        expected->ride.distance_mm.value_received_ms);
    CHECK(actual->fall.incident.session_id == expected->fall.incident.session_id);
    CHECK(actual->fall.incident.incident_id == expected->fall.incident.incident_id);
    check_report_equal(&actual->fall.last_report, &expected->fall.last_report);
    CHECK(actual->fall.phase == expected->fall.phase);
    check_report_equal(&actual->health.report, &expected->health.report);
    CHECK(actual->health.status == expected->health.status);
    CHECK(actual->reachability.last_valid_rx_ms == expected->reachability.last_valid_rx_ms);
    CHECK(actual->reachability.seen == expected->reachability.seen);
}

static void check_window_equal(
    const nostos_rx_window_t *actual,
    const nostos_rx_window_t *expected)
{
    CHECK(actual->session_id == expected->session_id);
    CHECK(actual->floor == expected->floor);
    CHECK(actual->highest == expected->highest);
    CHECK(actual->seen == expected->seen);
    CHECK(actual->approved == expected->approved);
    CHECK(actual->started == expected->started);
}

static void check_message_equal(
    const nostos_message_t *actual,
    const nostos_message_t *expected)
{
    CHECK(actual->type == expected->type);
    CHECK(actual->source_id == expected->source_id);
    CHECK(actual->session_id == expected->session_id);
    CHECK(actual->sequence == expected->sequence);
    CHECK(actual->payload.environment.temperature_c_x10 ==
        expected->payload.environment.temperature_c_x10);
    CHECK(actual->payload.environment.humidity_pct_x10 ==
        expected->payload.environment.humidity_pct_x10);
    CHECK(actual->payload.environment.temperature_quality ==
        expected->payload.environment.temperature_quality);
    CHECK(actual->payload.environment.humidity_quality ==
        expected->payload.environment.humidity_quality);
    CHECK(actual->payload.ride.valid == expected->payload.ride.valid);
    CHECK(actual->payload.ride.kmh_x10 == expected->payload.ride.kmh_x10);
    CHECK(actual->payload.ride.distance_mm == expected->payload.ride.distance_mm);
    CHECK(actual->payload.incident.session_id ==
        expected->payload.incident.session_id);
    CHECK(actual->payload.incident.incident_id ==
        expected->payload.incident.incident_id);
    CHECK(actual->payload.status == expected->payload.status);
    CHECK(actual->payload.ack.source_id == expected->payload.ack.source_id);
    CHECK(actual->payload.ack.type == expected->payload.ack.type);
    CHECK(actual->payload.ack.session_id == expected->payload.ack.session_id);
    CHECK(actual->payload.ack.sequence == expected->payload.ack.sequence);
    CHECK(actual->payload.ack.result == expected->payload.ack.result);
}

static void check_request_slot_empty(const nostos_request_slot_t *slot)
{
    const nostos_message_t empty_message = {0};
    CHECK(!slot->pending);
    check_message_equal(&slot->message, &empty_message);
}

static void check_incident_equal(
    const nostos_incident_record_t *actual,
    const nostos_incident_record_t *expected)
{
    CHECK(actual->source_id == expected->source_id);
    CHECK(actual->kind == expected->kind);
    CHECK(actual->ref.session_id == expected->ref.session_id);
    CHECK(actual->ref.incident_id == expected->ref.incident_id);
    CHECK(actual->used == expected->used);
    CHECK(actual->closed == expected->closed);
    CHECK(actual->muted == expected->muted);
}

static void new_session_clears_only_replaced_source(void)
{
    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 10U, 0U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 3U, 20U, 0U) == NOSTOS_OK);

    nostos_message_t message = {
        .type = NOSTOS_ENVIRONMENT,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = 1U,
        .payload.environment = {
            .temperature_c_x10 = 250,
            .humidity_pct_x10 = 500U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        },
    };
    CHECK(nostos_receiver_apply(&receiver, &message, 10U) == NOSTOS_OK);
    message.type = NOSTOS_RIDE;
    message.sequence = 2U;
    message.payload.ride = (nostos_ride_t){true, 123U, 4567U};
    CHECK(nostos_receiver_apply(&receiver, &message, 20U) == NOSTOS_OK);
    message.type = NOSTOS_FALL;
    message.sequence = 3U;
    message.payload.incident = (nostos_incident_ref_t){10U, 1U};
    CHECK(nostos_receiver_apply(&receiver, &message, 30U) == NOSTOS_OK);
    size_t source2_incident_index = NOSTOS_INCIDENT_CAPACITY;
    for (size_t index = 0U; index < NOSTOS_INCIDENT_CAPACITY; ++index) {
        if (receiver.incidents[index].used &&
            receiver.incidents[index].source_id == 2U) {
            source2_incident_index = index;
            break;
        }
    }
    CHECK(source2_incident_index < NOSTOS_INCIDENT_CAPACITY);
    message.type = NOSTOS_SPEED_DOWN;
    message.sequence = 4U;
    CHECK(nostos_receiver_apply(&receiver, &message, 40U) == NOSTOS_OK);
    message.type = NOSTOS_STOP;
    message.sequence = 5U;
    CHECK(nostos_receiver_apply(&receiver, &message, 50U) == NOSTOS_OK);

    message = (nostos_message_t){
        .type = NOSTOS_ENVIRONMENT,
        .source_id = 3U,
        .session_id = 20U,
        .sequence = 1U,
        .payload.environment = {
            .temperature_c_x10 = 300,
            .humidity_pct_x10 = 600U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        },
    };
    CHECK(nostos_receiver_apply(&receiver, &message, 60U) == NOSTOS_OK);
    message.type = NOSTOS_RIDE;
    message.sequence = 2U;
    message.payload.ride = (nostos_ride_t){true, 234U, 5678U};
    CHECK(nostos_receiver_apply(&receiver, &message, 70U) == NOSTOS_OK);
    message.type = NOSTOS_FALL;
    message.sequence = 3U;
    message.payload.incident = (nostos_incident_ref_t){20U, 1U};
    CHECK(nostos_receiver_apply(&receiver, &message, 80U) == NOSTOS_OK);
    size_t source3_incident_index = NOSTOS_INCIDENT_CAPACITY;
    for (size_t index = 0U; index < NOSTOS_INCIDENT_CAPACITY; ++index) {
        if (receiver.incidents[index].used &&
            receiver.incidents[index].source_id == 3U) {
            source3_incident_index = index;
            break;
        }
    }
    CHECK(source3_incident_index < NOSTOS_INCIDENT_CAPACITY);

    nostos_node_state_t source3_node = receiver.shared_data.nodes[2];
    nostos_rx_window_t source3_window = receiver.windows[2];
    nostos_incident_record_t source3_incident =
        receiver.incidents[source3_incident_index];
    nostos_outputs_t outputs = nostos_receiver_outputs(&receiver, 80U);
    CHECK(outputs.led == NOSTOS_LED_RED_BLINK);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_EMERGENCY);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 11U, 7U) == NOSTOS_OK);

    const nostos_node_state_t empty_source2 = {.source_id = 2U};
    check_node_equal(&receiver.shared_data.nodes[1], &empty_source2);
    check_request_slot_empty(&receiver.pending_stop);
    check_request_slot_empty(&receiver.pending_button);
    CHECK(receiver.windows[1].approved);
    CHECK(receiver.windows[1].session_id == 11U);
    CHECK(receiver.windows[1].floor == 7U);
    CHECK(!receiver.windows[1].started);
    check_node_equal(&receiver.shared_data.nodes[2], &source3_node);
    check_window_equal(&receiver.windows[2], &source3_window);

    const nostos_incident_record_t *cleared =
        &receiver.incidents[source2_incident_index];
    CHECK(cleared->source_id == 0U);
    CHECK(cleared->kind == 0U);
    CHECK(cleared->ref.session_id == 0U);
    CHECK(cleared->ref.incident_id == 0U);
    CHECK(!cleared->used);
    CHECK(!cleared->closed);
    CHECK(!cleared->muted);
    check_incident_equal(&receiver.incidents[source3_incident_index],
        &source3_incident);

    size_t source2_incidents = 0U;
    size_t source3_incidents = 0U;
    for (size_t index = 0U; index < NOSTOS_INCIDENT_CAPACITY; ++index) {
        const nostos_incident_record_t *incident = &receiver.incidents[index];
        if (incident->used && incident->source_id == 2U) ++source2_incidents;
        if (incident->used && incident->source_id == 3U) ++source3_incidents;
    }
    CHECK(source2_incidents == 0U);
    CHECK(source3_incidents == 1U);
    outputs = nostos_receiver_outputs(&receiver, 81U);
    CHECK(outputs.led == NOSTOS_LED_RED_BLINK);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_EMERGENCY);

    /* Replacement rejects the old epoch, enforces the new floor inclusively,
     * and leaves the other source replay window unchanged. */
    nostos_message_t boundary = {
        .type = NOSTOS_HEARTBEAT,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = 99U,
    };
    CHECK(nostos_receiver_apply(&receiver, &boundary, 82U) ==
        NOSTOS_SESSION_REQUIRED);
    boundary.session_id = 11U;
    boundary.sequence = 6U;
    CHECK(nostos_receiver_apply(&receiver, &boundary, 83U) == NOSTOS_STALE);
    boundary.sequence = 7U;
    CHECK(nostos_receiver_apply(&receiver, &boundary, 84U) == NOSTOS_OK);

    nostos_message_t source3_replay = {
        .type = NOSTOS_FALL,
        .source_id = 3U,
        .session_id = 20U,
        .sequence = 3U,
        .payload.incident = {20U, 1U},
    };
    CHECK(nostos_receiver_apply(&receiver, &source3_replay, 85U) ==
        NOSTOS_DUPLICATE);
    check_window_equal(&receiver.windows[2], &source3_window);

    /* A later replacement of source 2 must not clear source 3 mailboxes. */
    message.type = NOSTOS_SPEED_UP;
    message.sequence = 4U;
    CHECK(nostos_receiver_apply(&receiver, &message, 90U) == NOSTOS_OK);
    message.type = NOSTOS_STOP;
    message.sequence = 5U;
    CHECK(nostos_receiver_apply(&receiver, &message, 100U) == NOSTOS_OK);
    nostos_message_t source3_button = receiver.pending_button.message;
    nostos_message_t source3_stop = receiver.pending_stop.message;
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 12U, 0U) == NOSTOS_OK);
    CHECK(receiver.pending_button.pending);
    check_message_equal(&receiver.pending_button.message, &source3_button);
    CHECK(receiver.pending_stop.pending);
    check_message_equal(&receiver.pending_stop.message, &source3_stop);
}

static void single_source_fall_ends_at_new_session(void)
{
    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 30U, 0U) == NOSTOS_OK);
    nostos_message_t fall = {
        .type = NOSTOS_FALL,
        .source_id = 2U,
        .session_id = 30U,
        .sequence = 1U,
        .payload.incident = {30U, 1U},
    };
    CHECK(nostos_receiver_apply(&receiver, &fall, 10U) == NOSTOS_OK);
    nostos_outputs_t outputs = nostos_receiver_outputs(&receiver, 10U);
    CHECK(outputs.led == NOSTOS_LED_RED_BLINK);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_EMERGENCY);

    CHECK(nostos_receiver_approve_session(&receiver, 2U, 31U, 0U) == NOSTOS_OK);
    outputs = nostos_receiver_outputs(&receiver, 11U);
    CHECK(outputs.led == NOSTOS_LED_OFF);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_OFF);
}

static void mixed_source_mailboxes_clear_selectively(void)
{
    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 40U, 0U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 3U, 50U, 0U) == NOSTOS_OK);

    nostos_message_t stop2 = {
        .type = NOSTOS_STOP,
        .source_id = 2U,
        .session_id = 40U,
        .sequence = 1U,
    };
    nostos_message_t button3 = {
        .type = NOSTOS_SPEED_UP,
        .source_id = 3U,
        .session_id = 50U,
        .sequence = 1U,
    };
    CHECK(nostos_receiver_apply(&receiver, &stop2, 10U) == NOSTOS_OK);
    CHECK(nostos_receiver_apply(&receiver, &button3, 11U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 41U, 0U) == NOSTOS_OK);
    check_request_slot_empty(&receiver.pending_stop);
    CHECK(receiver.pending_button.pending);
    check_message_equal(&receiver.pending_button.message, &button3);

    nostos_receiver_clear_requests(&receiver);
    nostos_message_t stop3 = {
        .type = NOSTOS_STOP,
        .source_id = 3U,
        .session_id = 50U,
        .sequence = 2U,
    };
    nostos_message_t button2 = {
        .type = NOSTOS_SPEED_DOWN,
        .source_id = 2U,
        .session_id = 41U,
        .sequence = 0U,
    };
    CHECK(nostos_receiver_apply(&receiver, &stop3, 20U) == NOSTOS_OK);
    CHECK(nostos_receiver_apply(&receiver, &button2, 21U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 42U, 0U) == NOSTOS_OK);
    check_request_slot_empty(&receiver.pending_button);
    CHECK(receiver.pending_stop.pending);
    check_message_equal(&receiver.pending_stop.message, &stop3);
}

static void ride_codec_and_shared_state(void)
{
    const uint8_t expected[] = {
        0x02U, 0x44U, 0x02U, 0x44U, 0x33U, 0x22U, 0x11U, 0x66U, 0x55U,
        0x01U, 0xE4U, 0x00U, 0x88U, 0x77U, 0x66U, 0x55U,
    };
    nostos_message_t message = {
        .type = NOSTOS_RIDE,
        .source_id = 2U,
        .session_id = 0x11223344U,
        .sequence = 0x5566U,
        .payload.ride = {
            .valid = true,
            .kmh_x10 = 228U,
            .distance_mm = UINT32_C(0x55667788),
        },
    };
    uint8_t wire[NOSTOS_WIRE_MAX] = {0};
    size_t length = 0U;
    CHECK(NOSTOS_TYPE_COUNT == 10U);
    CHECK(nostos_type_info(NOSTOS_RIDE) != NULL);
    CHECK(nostos_type_info(NOSTOS_RIDE)->payload_size == 7U);
    CHECK(strcmp(nostos_type_info(NOSTOS_RIDE)->name, "RIDE") == 0);
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) == NOSTOS_OK);
    CHECK(length == sizeof(expected));
    CHECK(memcmp(wire, expected, sizeof(expected)) == 0);

    nostos_message_t decoded = {0};
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.ride.valid);
    CHECK(decoded.payload.ride.kmh_x10 == 228U);
    CHECK(decoded.payload.ride.distance_mm == UINT32_C(0x55667788));

    memset(wire, 0xA5, sizeof(wire));
    uint8_t unchanged[NOSTOS_WIRE_MAX];
    memcpy(unchanged, wire, sizeof(unchanged));
    length = 77U;
    message.payload.ride = (nostos_ride_t){.valid = false, .kmh_x10 = 1U};
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    message.payload.ride = (nostos_ride_t){.valid = false, .distance_mm = 1U};
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    CHECK(length == 77U);
    CHECK(memcmp(wire, unchanged, sizeof(wire)) == 0);

    memcpy(wire, expected, sizeof(expected));
    wire[NOSTOS_HEADER_SIZE] = 0U;
    decoded = (nostos_message_t){.type = 0xEEU};
    CHECK(nostos_message_decode(wire, sizeof(expected), &decoded) == NOSTOS_BAD_VALUE);
    CHECK(decoded.type == 0xEEU);

    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 700U, 0U) == NOSTOS_OK);
    message = (nostos_message_t){
        .type = NOSTOS_RIDE,
        .source_id = 2U,
        .session_id = 700U,
        .sequence = 12U,
        .payload.ride = {
            .valid = true,
            .kmh_x10 = 315U,
            .distance_mm = 123456U,
        },
    };
    CHECK(nostos_receiver_apply(&receiver, &message, 100U) == NOSTOS_OK);
    nostos_ride_state_t *state = &receiver.shared_data.nodes[1].ride;
    CHECK(state->speed_kmh_x10.quality == NOSTOS_VALID);
    CHECK(state->distance_mm.quality == NOSTOS_VALID);
    CHECK(state->speed_kmh_x10.has_value);
    CHECK(state->distance_mm.has_value);
    CHECK(state->speed_kmh_x10.value == 315U);
    CHECK(state->distance_mm.value == 123456U);
    CHECK(state->speed_kmh_x10.value_received_ms == 100U);
    CHECK(state->distance_mm.value_received_ms == 100U);

    nostos_message_t heartbeat = {
        .type = NOSTOS_HEARTBEAT,
        .source_id = 2U,
        .session_id = 700U,
        .sequence = 13U,
        .payload.status = 0U,
    };
    CHECK(nostos_receiver_apply(&receiver, &heartbeat, 110U) == NOSTOS_OK);
    message.sequence = 11U;
    CHECK(nostos_receiver_apply(&receiver, &message, 120U) == NOSTOS_STALE);
    CHECK(state->report.sequence == 12U);

    message.sequence = 14U;
    message.payload.ride = (nostos_ride_t){0};
    CHECK(nostos_receiver_apply(&receiver, &message, 200U) == NOSTOS_OK);
    CHECK(state->speed_kmh_x10.quality == NOSTOS_UNMEASURED);
    CHECK(state->distance_mm.quality == NOSTOS_UNMEASURED);
    CHECK(state->speed_kmh_x10.value == 315U);
    CHECK(state->distance_mm.value == 123456U);
    CHECK(state->speed_kmh_x10.value_received_ms == 100U);
    CHECK(state->distance_mm.value_received_ms == 100U);
    CHECK(nostos_report_fresh(&state->report, 3200U, NOSTOS_FRESH_MS));
    CHECK(!nostos_report_fresh(&state->report, 3201U, NOSTOS_FRESH_MS));
}

static void shared_data_request_codec_and_mailbox(void)
{
    const uint8_t expected[] = {
        0x02U, 0x46U, 0x02U, 0x44U, 0x33U, 0x22U, 0x11U, 0x66U, 0x55U,
        NOSTOS_SHARED_DATA_MASK,
    };
    nostos_message_t message = {
        .type = NOSTOS_SHARED_DATA_REQUEST,
        .source_id = 2U,
        .session_id = UINT32_C(0x11223344),
        .sequence = 0x5566U,
        .payload.shared_data_request = {NOSTOS_SHARED_DATA_MASK},
    };
    uint8_t wire[NOSTOS_WIRE_MAX] = {0};
    size_t length = 0U;
    const nostos_type_info_t *info =
        nostos_type_info(NOSTOS_SHARED_DATA_REQUEST);
    CHECK(info != NULL);
    CHECK(info->payload_size == 1U);
    CHECK(strcmp(info->name, "SHARED_DATA_REQUEST") == 0);
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) ==
        NOSTOS_OK);
    CHECK(length == sizeof(expected));
    CHECK(memcmp(wire, expected, sizeof(expected)) == 0);

    nostos_message_t decoded = {0};
    CHECK(nostos_message_decode(wire, length, &decoded) == NOSTOS_OK);
    CHECK(decoded.payload.shared_data_request.mask == NOSTOS_SHARED_DATA_MASK);

    message.payload.shared_data_request.mask = 0U;
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    message.payload.shared_data_request.mask = 1U << 2;
    CHECK(nostos_message_encode(&message, wire, sizeof(wire), &length) ==
        NOSTOS_BAD_VALUE);
    memcpy(wire, expected, sizeof(expected));
    wire[NOSTOS_HEADER_SIZE] = 0U;
    CHECK(nostos_message_decode(wire, sizeof(expected), &decoded) ==
        NOSTOS_BAD_VALUE);
    wire[NOSTOS_HEADER_SIZE] = 1U << 2;
    CHECK(nostos_message_decode(wire, sizeof(expected), &decoded) ==
        NOSTOS_BAD_VALUE);

    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 10U, 0U) ==
        NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 3U, 20U, 0U) ==
        NOSTOS_OK);
    message = (nostos_message_t){
        .type = NOSTOS_SHARED_DATA_REQUEST,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = 1U,
        .payload.shared_data_request = {NOSTOS_SHARED_DATA_RIDE},
    };
    CHECK(nostos_receiver_apply(&receiver, &message, 10U) == NOSTOS_OK);
    message.source_id = 3U;
    message.session_id = 20U;
    message.payload.shared_data_request.mask = NOSTOS_SHARED_DATA_ENVIRONMENT;
    CHECK(nostos_receiver_apply(&receiver, &message, 20U) == NOSTOS_OK);
    CHECK(nostos_receiver_take_shared_data_request(&receiver) ==
        NOSTOS_SHARED_DATA_MASK);
    CHECK(nostos_receiver_take_shared_data_request(&receiver) == 0U);

    message.source_id = 2U;
    message.session_id = 10U;
    message.sequence = 2U;
    message.payload.shared_data_request.mask = NOSTOS_SHARED_DATA_RIDE;
    CHECK(nostos_receiver_apply(&receiver, &message, 30U) == NOSTOS_OK);
    message.source_id = 3U;
    message.session_id = 20U;
    message.sequence = 2U;
    message.payload.shared_data_request.mask = NOSTOS_SHARED_DATA_ENVIRONMENT;
    CHECK(nostos_receiver_apply(&receiver, &message, 40U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 11U, 0U) ==
        NOSTOS_OK);
    CHECK(receiver.pending_shared_data_requests[1] == 0U);
    CHECK(receiver.pending_shared_data_requests[2] ==
        NOSTOS_SHARED_DATA_ENVIRONMENT);
    nostos_receiver_clear_requests(&receiver);
    CHECK(nostos_receiver_take_shared_data_request(&receiver) ==
        NOSTOS_SHARED_DATA_ENVIRONMENT);
}

static void retired_application_types_are_rejected(void)
{
    const uint8_t retired[] = {0x12U, 0x20U, 0x21U, 0x22U,
                               0x31U, 0x40U, 0x43U, 0x45U};
    const nostos_peer_t peers[NOSTOS_NODE_COUNT] = {
        {0x0101U, 1U, 1U},
        {0x0202U, 2U, 2U},
        {0x0303U, 3U, 3U},
    };
    nostos_bridge_t bridge;
    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);

    uint8_t wire[NOSTOS_HEADER_SIZE] = {
        NOSTOS_VERSION, 0U, 1U, 1U, 0U, 0U, 0U, 0U, 0U,
    };
    for (size_t index = 0U; index < sizeof(retired); ++index) {
        wire[1] = retired[index];
        nostos_message_t decoded = {.type = 0xEEU};
        CHECK(nostos_type_info(retired[index]) == NULL);
        CHECK(nostos_message_decode(wire, sizeof(wire), &decoded) ==
            NOSTOS_UNSUPPORTED_TYPE);
        CHECK(decoded.type == 0xEEU);
        CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire,
            sizeof(wire), 0U, (uint32_t)index, true) == NOSTOS_UNSUPPORTED_TYPE);
    }
    CHECK(bridge.count == 0U);
}

static void bridge_accepts_sensor_data_from_authenticated_sources(void)
{
    const nostos_peer_t peers[NOSTOS_NODE_COUNT] = {
        {0x0101U, 1U, 1U},
        {0x0202U, 2U, 2U},
        {0x0303U, 3U, 3U},
    };
    nostos_bridge_t bridge;
    nostos_job_t job;
    uint8_t wire[NOSTOS_WIRE_MAX];

    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);
    size_t length = encode_sensor_message(NOSTOS_RIDE, 1U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U,
        10U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 11U, true, &job) == NOSTOS_OK);

    length = encode_sensor_message(NOSTOS_ENVIRONMENT, 1U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U,
        12U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 13U, true, &job) == NOSTOS_OK);
    length = encode_sensor_message(NOSTOS_ENVIRONMENT, 3U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length, 0x0303U,
        14U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 15U, true, &job) == NOSTOS_OK);
    length = encode_sensor_message(NOSTOS_RIDE, 2U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length, 0x0202U,
        16U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 17U, true, &job) == NOSTOS_OK);

    CHECK(nostos_bridge_init(&bridge, 3U, peers) == NOSTOS_OK);
    length = encode_sensor_message(NOSTOS_ENVIRONMENT, 3U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U,
        18U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 19U, true, &job) == NOSTOS_OK);
    length = encode_sensor_message(NOSTOS_RIDE, 3U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U,
        20U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 21U, true, &job) == NOSTOS_OK);
    length = encode_sensor_message(NOSTOS_RIDE, 1U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length, 0x0101U,
        22U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 23U, true, &job) == NOSTOS_OK);
    length = encode_sensor_message(NOSTOS_ENVIRONMENT, 2U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_UART, wire, length, 0x0202U,
        24U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 25U, true, &job) == NOSTOS_OK);

    length = encode_sensor_message(NOSTOS_ENVIRONMENT, 1U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U,
        26U, true) == NOSTOS_UNAUTHORIZED);
    CHECK(bridge.count == 0U);
}

static void fall_is_the_only_emergency_output(void)
{
    nostos_receiver_t receiver;
    CHECK(nostos_receiver_init(&receiver, 1U) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&receiver, 2U, 800U, 0U) == NOSTOS_OK);
    nostos_message_t message = {
        .type = NOSTOS_FALL,
        .source_id = 2U,
        .session_id = 800U,
        .sequence = 1U,
        .payload.incident = {800U, 1U},
    };
    CHECK(nostos_receiver_apply(&receiver, &message, 100U) == NOSTOS_OK);
    nostos_outputs_t outputs = nostos_receiver_outputs(&receiver, 100U);
    CHECK(outputs.led == NOSTOS_LED_RED_BLINK);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_EMERGENCY);

    CHECK(nostos_receiver_mute(&receiver, 2U, NOSTOS_FALL,
        message.payload.incident) == NOSTOS_OK);
    outputs = nostos_receiver_outputs(&receiver, 101U);
    CHECK(outputs.led == NOSTOS_LED_RED_BLINK);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_OFF);

    message.type = NOSTOS_FALL_CLEAR;
    message.sequence = 2U;
    CHECK(nostos_receiver_apply(&receiver, &message, 102U) == NOSTOS_OK);
    outputs = nostos_receiver_outputs(&receiver, 102U);
    CHECK(outputs.led == NOSTOS_LED_OFF);
    CHECK(outputs.buzzer == NOSTOS_BUZZER_OFF);
}

static void strict_bridge_priority_and_expiry(void)
{
    const nostos_peer_t peers[NOSTOS_NODE_COUNT] = {
        {0x0101U, 1U, 1U},
        {0x0202U, 2U, 2U},
        {0x0303U, 3U, 3U},
    };
    nostos_bridge_t bridge;
    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);

    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = encode_message(NOSTOS_SPEED_UP, 1U, 90U, 1U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U, 10U, true) == NOSTOS_OK);
    length = encode_message(NOSTOS_STOP, 1U, 90U, 2U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U, 11U, true) == NOSTOS_OK);
    for (uint16_t sequence = 3U; sequence < 8U; ++sequence)
    {
        length = encode_message(NOSTOS_FALL, 1U, 90U, sequence, wire);
        CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U,
                                   (uint32_t)(10U + sequence), true) == NOSTOS_OK);
    }

    nostos_job_t job;
    for (unsigned index = 0U; index < 5U; ++index)
    {
        CHECK(nostos_bridge_next(&bridge, 100U, true, &job) == NOSTOS_OK);
        CHECK(job.wire[1] == NOSTOS_FALL);
    }
    CHECK(nostos_bridge_next(&bridge, 100U, true, &job) == NOSTOS_OK);
    CHECK(job.wire[1] == NOSTOS_STOP);
    CHECK(nostos_bridge_next(&bridge, 100U, true, &job) == NOSTOS_OK);
    CHECK(job.wire[1] == NOSTOS_SPEED_UP);

    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);
    length = encode_message(NOSTOS_STOP, 1U, 91U, 1U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length, 0U, 0U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, NOSTOS_BRIDGE_MAX_AGE_MS + 1U,
                             true, &job) == NOSTOS_EXPIRED);
}

static void bridge_reserves_stop_and_fall_capacity(void)
{
    const nostos_peer_t peers[NOSTOS_NODE_COUNT] = {
        {0x0101U, 1U, 1U},
        {0x0202U, 2U, 2U},
        {0x0303U, 3U, 3U},
    };
    nostos_bridge_t bridge;
    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);

    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    for (uint16_t sequence = 0U;
         sequence < NOSTOS_BRIDGE_NORMAL_CAPACITY; ++sequence) {
        length = encode_message(
            NOSTOS_SPEED_UP, 1U, 92U, sequence, wire);
        CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
            0U, sequence, true) == NOSTOS_OK);
    }
    length = encode_message(
        NOSTOS_SPEED_UP, 1U, 92U,
        (uint16_t)NOSTOS_BRIDGE_NORMAL_CAPACITY, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
        0U, 20U, true) == NOSTOS_FULL);

    length = encode_message(NOSTOS_STOP, 1U, 92U, 20U, wire);
    CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
        0U, 21U, true) == NOSTOS_OK);
    for (uint16_t sequence = 21U; sequence < 25U; ++sequence) {
        length = encode_message(NOSTOS_FALL, 1U, 92U, sequence, wire);
        CHECK(nostos_bridge_accept(&bridge, NOSTOS_TO_MESH, wire, length,
            0U, sequence, true) == NOSTOS_OK);
    }
    CHECK(bridge.count == NOSTOS_BRIDGE_CAPACITY);
}

typedef struct
{
    unsigned audio_plays;
    unsigned audio_stops;
    unsigned output_updates;
    bool audio_playing;
    uint8_t last_audio_type;
    nostos_outputs_t outputs;
} endpoint_fixture_t;

static bool fixture_uart_send(void *context, const uint8_t *frame, size_t length)
{
    (void)context;
    return (frame != NULL) && (length > 0U);
}

static void fixture_outputs(void *context, nostos_outputs_t outputs)
{
    endpoint_fixture_t *fixture = context;
    ++fixture->output_updates;
    fixture->outputs = outputs;
}

static bool fixture_audio_playing(void *context)
{
    endpoint_fixture_t *fixture = context;
    return fixture->audio_playing;
}

static bool fixture_audio_stop(void *context)
{
    endpoint_fixture_t *fixture = context;
    ++fixture->audio_stops;
    fixture->audio_playing = false;
    return true;
}

static bool fixture_audio_play(
    void *context,
    const nostos_message_t *message)
{
    endpoint_fixture_t *fixture = context;
    CHECK(message != NULL);
    ++fixture->audio_plays;
    fixture->audio_playing = true;
    fixture->last_audio_type = message->type;
    return true;
}

static void simplified_request_scheduler(void)
{
    endpoint_fixture_t fixture = {0};
    const nostos_endpoint_io_t io = {
        .context = &fixture,
        .uart_send = fixture_uart_send,
        .outputs = fixture_outputs,
        .audio_playing = fixture_audio_playing,
        .audio_stop = fixture_audio_stop,
        .audio_play = fixture_audio_play,
    };
    nostos_endpoint_t endpoint;
    CHECK(nostos_endpoint_init(&endpoint, 1U, 100U, &io) == NOSTOS_OK);
    CHECK(nostos_receiver_approve_session(&endpoint.receiver, 2U, 200U, 0U) == NOSTOS_OK);

    nostos_message_t request = {
        .type = NOSTOS_SPEED_DOWN,
        .source_id = 2U,
        .session_id = 200U,
        .sequence = 1U,
    };
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 10U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 10U);
    CHECK(fixture.audio_plays == 1U);
    CHECK(fixture.last_audio_type == NOSTOS_SPEED_DOWN);
    CHECK(fixture.audio_playing);
    CHECK(!endpoint.receiver.pending_button.pending);

    /* BTN1/2 received while audio is busy is consumed and never replayed. */
    request.type = NOSTOS_SPEED_UP;
    request.sequence = 2U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 20U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 20U);
    CHECK(endpoint.dropped_busy_buttons == 1U);
    CHECK(fixture.audio_plays == 1U);
    CHECK(!endpoint.receiver.pending_button.pending);

    /* The one-slot mailbox rejects a second BTN but still consumes its
     * sequence, so retransmission is a duplicate instead of a late replay. */
    fixture.audio_playing = false;
    request.type = NOSTOS_SPEED_DOWN;
    request.sequence = 3U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 30U) == NOSTOS_OK);
    request.type = NOSTOS_SPEED_UP;
    request.sequence = 4U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 31U) == NOSTOS_FULL);
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 32U) == NOSTOS_DUPLICATE);
    CHECK(endpoint.receiver.pending_button.pending);
    CHECK(endpoint.receiver.pending_button.message.type == NOSTOS_SPEED_DOWN);

    /* STOP removes the pending BTN and preempts currently playing audio. */
    fixture.audio_playing = true;
    request.type = NOSTOS_STOP;
    request.sequence = 5U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 40U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 40U);
    CHECK(endpoint.stop_preemptions == 1U);
    CHECK(fixture.audio_stops == 1U);
    CHECK(fixture.audio_plays == 2U);
    CHECK(fixture.last_audio_type == NOSTOS_STOP);
    CHECK(!endpoint.receiver.pending_stop.pending);
    CHECK(!endpoint.receiver.pending_button.pending);

    /* FALL owns all outputs: stop playing audio, clear pending work, and do
     * not accept audio work until its matching CLEAR has been applied. */
    request.type = NOSTOS_SPEED_DOWN;
    request.sequence = 6U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 50U) == NOSTOS_OK);
    nostos_message_t fall = {
        .type = NOSTOS_FALL,
        .source_id = 2U,
        .session_id = 200U,
        .sequence = 7U,
        .payload.incident = {200U, 1U},
    };
    CHECK(nostos_receiver_apply(&endpoint.receiver, &fall, 51U) == NOSTOS_OK);
    /* Muting only the buzzer must not lower FALL scheduling priority. */
    CHECK(nostos_receiver_mute(&endpoint.receiver, 2U, NOSTOS_FALL,
        fall.payload.incident) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 51U);
    CHECK(endpoint.fall_preemptions == 1U);
    CHECK(fixture.audio_stops == 2U);
    CHECK(!fixture.audio_playing);
    CHECK(!endpoint.receiver.pending_stop.pending);
    CHECK(!endpoint.receiver.pending_button.pending);
    CHECK(fixture.outputs.led == NOSTOS_LED_RED_BLINK);
    CHECK(fixture.outputs.buzzer == NOSTOS_BUZZER_OFF);

    request.type = NOSTOS_SPEED_UP;
    request.sequence = 8U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 60U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 60U);
    CHECK(!endpoint.receiver.pending_button.pending);
    CHECK(fixture.audio_plays == 2U);

    request.type = NOSTOS_STOP;
    request.sequence = 9U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 61U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 61U);
    CHECK(!endpoint.receiver.pending_stop.pending);
    CHECK(fixture.audio_plays == 2U);

    nostos_message_t clear = {
        .type = NOSTOS_FALL_CLEAR,
        .source_id = 2U,
        .session_id = 200U,
        .sequence = 10U,
        .payload.incident = {200U, 1U},
    };
    CHECK(nostos_receiver_apply(&endpoint.receiver, &clear, 70U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 70U);
    CHECK(fixture.outputs.led == NOSTOS_LED_OFF);
    CHECK(fixture.outputs.buzzer == NOSTOS_BUZZER_OFF);

    request.type = NOSTOS_SPEED_UP;
    request.sequence = 11U;
    CHECK(nostos_receiver_apply(&endpoint.receiver, &request, 80U) == NOSTOS_OK);
    nostos_endpoint_process(&endpoint, 80U);
    CHECK(fixture.audio_plays == 3U);
    CHECK(fixture.last_audio_type == NOSTOS_SPEED_UP);
}

int main(void)
{
    crc_and_frame_validation();
    session_and_sequence_replay_protection();
    new_session_clears_only_replaced_source();
    single_source_fall_ends_at_new_session();
    mixed_source_mailboxes_clear_selectively();
    ride_codec_and_shared_state();
    shared_data_request_codec_and_mailbox();
    retired_application_types_are_rejected();
    bridge_accepts_sensor_data_from_authenticated_sources();
    fall_is_the_only_emergency_output();
    strict_bridge_priority_and_expiry();
    bridge_reserves_stop_and_fall_capacity();
    simplified_request_scheduler();
    puts("PASS v2 CRC, trusted replay rejection, strict bridge and request priority");
    return 0;
}
