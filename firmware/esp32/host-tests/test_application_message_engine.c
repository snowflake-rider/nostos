#include "application_message_engine.h"
#include "application_event_heap.h"
#include "check.h"
#include "official_packet_writer.h"

#include <limits.h>
#include <stdio.h>

static size_t encode_message(
    const nostos_message_t *message,
    uint8_t wire[NOSTOS_WIRE_MAX])
{
    size_t length = 0U;
    CHECK(nostos_message_encode(
        message, wire, NOSTOS_WIRE_MAX, &length) == NOSTOS_OK);
    return length;
}

static sensor_link_message_t decode_sensor_frame(
    const uint8_t *frame,
    size_t length)
{
    sensor_link_parser_t parser = {0};
    sensor_link_message_t message = {0};
    sensor_link_result_t result = SENSOR_LINK_EMPTY;
    for (size_t i = 0U; i < length; ++i) {
        result = sensor_link_feed(
            &parser, frame[i], (uint32_t)i, &message);
    }
    CHECK(result == SENSOR_LINK_OK);
    return message;
}

static void check_local_and_remote_same_engine(void)
{
    application_message_engine_t engine;
    official_packet_writer_t writer;
    CHECK(application_message_engine_init(&engine, 1U, 100U) == NOSTOS_OK);
    CHECK(official_packet_writer_init(&writer, 1U, 100U) == NOSTOS_OK);

    uint8_t local_wire[NOSTOS_WIRE_MAX];
    size_t local_length = 0U;
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_SPEED_UP, local_wire, &local_length) == NOSTOS_OK);
    nostos_message_t local_accepted;
    CHECK(application_message_engine_accept_wire(
        &engine, local_wire, local_length, 10U,
        &local_accepted) == NOSTOS_OK);

    uint8_t output[SENSOR_LINK_FRAME_SIZE];
    size_t output_length = 0U;
    uint32_t command_id = 0U;
    CHECK(application_message_engine_encode_output(
        &engine, &local_accepted, output, &output_length,
        &command_id) == NOSTOS_OK);
    sensor_link_message_t local_output = decode_sensor_frame(
        output, output_length);
    CHECK(command_id == 1U);
    CHECK(local_output.type == SENSOR_LINK_OUTPUT_EVENT);
    CHECK(local_output.output_event.command_id == 1U);
    CHECK(local_output.output_event.source_id == 1U);
    CHECK(local_output.output_event.event_type == SENSOR_LINK_EVENT_SPEED_UP);

    const nostos_message_t remote = {
        .type = NOSTOS_SPEED_UP,
        .source_id = 2U,
        .session_id = 200U,
        .sequence = 7U,
    };
    uint8_t remote_wire[NOSTOS_WIRE_MAX];
    size_t remote_length = encode_message(&remote, remote_wire);
    CHECK(application_message_engine_approve_authenticated_session(
        &engine, 2U, 200U, 7U) == NOSTOS_OK);
    nostos_message_t remote_accepted;
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 11U,
        &remote_accepted) == NOSTOS_OK);
    CHECK(application_message_engine_encode_output(
        &engine, &remote_accepted, output, &output_length,
        &command_id) == NOSTOS_OK);
    sensor_link_message_t remote_output = decode_sensor_frame(
        output, output_length);
    CHECK(command_id == 2U);
    CHECK(remote_output.output_event.event_type ==
        local_output.output_event.event_type);
    CHECK(remote_output.output_event.source_id == 2U);

    /* Replay is rejected by the same receiver and therefore cannot allocate
     * a second STM output command. */
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 12U,
        &remote_accepted) == NOSTOS_DUPLICATE);
    CHECK(engine.next_command_id == 3U);
    CHECK(engine.stats.accepted == 2U);
    CHECK(engine.stats.duplicate == 1U);
}

static void check_priority_fall_stop_buttons(void)
{
    application_event_heap_t heap;
    application_event_heap_init(&heap);
    const nostos_message_t button = {
        .type = NOSTOS_SPEED_DOWN,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = 1U,
    };
    const nostos_message_t stop = {
        .type = NOSTOS_STOP,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = 2U,
    };
    const nostos_message_t fall = {
        .type = NOSTOS_FALL,
        .source_id = 2U,
        .session_id = 10U,
        .sequence = 3U,
        .payload.incident = {.session_id = 10U, .incident_id = 1U},
    };
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = encode_message(&button, wire);
    CHECK(application_event_heap_push(
        &heap, wire, length, 0U) == NOSTOS_OK);
    length = encode_message(&stop, wire);
    CHECK(application_event_heap_push(
        &heap, wire, length, 0U) == NOSTOS_OK);
    length = encode_message(&fall, wire);
    CHECK(application_event_heap_push(
        &heap, wire, length, 0U) == NOSTOS_OK);

    nostos_job_t job;
    nostos_message_t decoded;
    CHECK(application_event_heap_pop(&heap, 1U, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(
        job.wire, job.length, &decoded) == NOSTOS_OK);
    CHECK(decoded.type == NOSTOS_FALL);
    CHECK(application_event_heap_pop(&heap, 1U, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(
        job.wire, job.length, &decoded) == NOSTOS_OK);
    CHECK(decoded.type == NOSTOS_STOP);
    CHECK(application_event_heap_pop(&heap, 1U, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(
        job.wire, job.length, &decoded) == NOSTOS_OK);
    CHECK(decoded.type == NOSTOS_SPEED_DOWN);
}

static void check_hello_ready_and_state_resync(void)
{
    application_message_engine_t engine;
    official_packet_writer_t writer;
    CHECK(application_message_engine_init(&engine, 1U, 300U) == NOSTOS_OK);
    CHECK(official_packet_writer_init(&writer, 1U, 300U) == NOSTOS_OK);

    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;
    nostos_message_t accepted;
    CHECK(official_packet_writer_ride(
        &writer, true, 321U, 98765U, wire, &length) == NOSTOS_OK);
    CHECK(application_message_engine_accept_wire(
        &engine, wire, length, 100U, &accepted) == NOSTOS_OK);
    CHECK(official_packet_writer_environment(
        &writer, 234, 567U, NOSTOS_VALID, NOSTOS_VALID,
        wire, &length) == NOSTOS_OK);
    CHECK(application_message_engine_accept_wire(
        &engine, wire, length, 101U, &accepted) == NOSTOS_OK);
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL, wire, &length) == NOSTOS_OK);
    CHECK(application_message_engine_accept_wire(
        &engine, wire, length, 102U, &accepted) == NOSTOS_OK);

    CHECK(application_message_engine_approve_authenticated_session(
        &engine, 2U, 301U, 5U) == NOSTOS_OK);
    const nostos_message_t remote_ride = {
        .type = NOSTOS_RIDE,
        .source_id = 2U,
        .session_id = 301U,
        .sequence = 5U,
        .payload.ride = {
            .valid = true,
            .kmh_x10 = 222U,
            .distance_mm = 44444U,
        },
    };
    length = encode_message(&remote_ride, wire);
    CHECK(application_message_engine_accept_wire(
        &engine, wire, length, 103U, &accepted) == NOSTOS_OK);
    const nostos_message_t remote_environment = {
        .type = NOSTOS_ENVIRONMENT,
        .source_id = 2U,
        .session_id = 301U,
        .sequence = 6U,
        .payload.environment = {
            .temperature_c_x10 = 250,
            .humidity_pct_x10 = 500U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        },
    };
    length = encode_message(&remote_environment, wire);
    CHECK(application_message_engine_accept_wire(
        &engine, wire, length, 104U, &accepted) == NOSTOS_OK);

    uint8_t ready[SENSOR_LINK_FRAME_SIZE];
    size_t ready_length = 0U;
    CHECK(sensor_link_encode_ready(
        300U, ready, &ready_length) == SENSOR_LINK_OK);
    sensor_link_message_t ready_message = decode_sensor_frame(
        ready, ready_length);
    CHECK(ready_message.type == SENSOR_LINK_READY);
    CHECK(ready_message.ready.command_epoch == 300U);

    nostos_message_t snapshots[APPLICATION_MESSAGE_SNAPSHOT_CAPACITY];
    size_t snapshot_count = 0U;
    CHECK(application_message_engine_snapshot(
        &engine, 105U, snapshots, &snapshot_count) == NOSTOS_OK);
    CHECK(snapshot_count == 5U);
    CHECK(snapshots[0].type == NOSTOS_FALL);
    CHECK(snapshots[1].type == NOSTOS_RIDE);
    CHECK(snapshots[1].payload.ride.kmh_x10 == 321U);
    CHECK(snapshots[1].payload.ride.distance_mm == 98765U);
    CHECK(snapshots[2].type == NOSTOS_ENVIRONMENT);
    CHECK(snapshots[2].payload.environment.temperature_c_x10 == 234);
    /* Official v2 environment encoding canonicalizes humidity to 0.5 %. */
    CHECK(snapshots[2].payload.environment.humidity_pct_x10 == 565U);
    CHECK(snapshots[3].type == NOSTOS_RIDE);
    CHECK(snapshots[3].source_id == 2U);
    CHECK(snapshots[3].payload.ride.kmh_x10 == 222U);
    CHECK(snapshots[4].type == NOSTOS_ENVIRONMENT);
    CHECK(snapshots[4].source_id == 2U);
    CHECK(snapshots[4].payload.environment.humidity_pct_x10 == 500U);
}

static void check_multi_source_fall_aggregate(void)
{
    application_message_engine_t engine;
    CHECK(application_message_engine_init(&engine, 1U, 500U) == NOSTOS_OK);
    CHECK(application_message_engine_approve_authenticated_session(
        &engine, 2U, 600U, 0U) == NOSTOS_OK);
    CHECK(application_message_engine_approve_authenticated_session(
        &engine, 3U, 700U, 0U) == NOSTOS_OK);

    const nostos_message_t messages[] = {
        {
            .type = NOSTOS_FALL,
            .source_id = 2U,
            .session_id = 600U,
            .sequence = 0U,
            .payload.incident = {.session_id = 600U, .incident_id = 1U},
        },
        {
            .type = NOSTOS_FALL,
            .source_id = 3U,
            .session_id = 700U,
            .sequence = 0U,
            .payload.incident = {.session_id = 700U, .incident_id = 1U},
        },
        {
            .type = NOSTOS_FALL_CLEAR,
            .source_id = 2U,
            .session_id = 600U,
            .sequence = 1U,
            .payload.incident = {.session_id = 600U, .incident_id = 1U},
        },
        {
            .type = NOSTOS_FALL_CLEAR,
            .source_id = 3U,
            .session_id = 700U,
            .sequence = 1U,
            .payload.incident = {.session_id = 700U, .incident_id = 1U},
        },
    };
    uint8_t wire[NOSTOS_WIRE_MAX];
    nostos_message_t accepted;
    for (size_t i = 0U; i < 3U; ++i) {
        size_t length = encode_message(&messages[i], wire);
        CHECK(application_message_engine_accept_wire(
            &engine, wire, length, (uint32_t)i, &accepted) == NOSTOS_OK);
    }

    uint8_t output[SENSOR_LINK_FRAME_SIZE];
    size_t output_length = 0U;
    uint32_t command_id = 0U;
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output, &output_length,
        &command_id) == NOSTOS_OK);
    sensor_link_message_t decoded = decode_sensor_frame(output, output_length);
    CHECK(decoded.output_event.event_type == SENSOR_LINK_EVENT_FALL);
    CHECK(decoded.output_event.source_id == 3U);

    size_t length = encode_message(&messages[3], wire);
    CHECK(application_message_engine_accept_wire(
        &engine, wire, length, 4U, &accepted) == NOSTOS_OK);
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output, &output_length,
        &command_id) == NOSTOS_OK);
    decoded = decode_sensor_frame(output, output_length);
    CHECK(decoded.output_event.event_type == SENSOR_LINK_EVENT_FALL_CLEAR);
}

static void check_command_id_and_result_counters(void)
{
    application_message_engine_t engine;
    CHECK(application_message_engine_init(&engine, 1U, 400U) == NOSTOS_OK);
    const nostos_message_t stop = {
        .type = NOSTOS_STOP,
        .source_id = 1U,
        .session_id = 400U,
        .sequence = 0U,
    };
    engine.next_command_id = UINT32_MAX;
    uint8_t output[SENSOR_LINK_FRAME_SIZE];
    size_t output_length = 0U;
    uint32_t command_id = 0U;
    CHECK(application_message_engine_encode_output(
        &engine, &stop, output, &output_length,
        &command_id) == NOSTOS_OK);
    CHECK(command_id == UINT32_MAX);
    CHECK(application_message_engine_encode_output(
        &engine, &stop, output, &output_length,
        &command_id) == NOSTOS_EXHAUSTED);

    application_message_engine_note_uart_tx(&engine, true);
    application_message_engine_note_uart_tx(&engine, false);
    const sensor_link_output_result_t accepted = {
        .command_id = UINT32_MAX,
        .status = SENSOR_LINK_OUTPUT_ACCEPTED,
    };
    const sensor_link_output_result_t hardware_error = {
        .command_id = UINT32_MAX,
        .status = SENSOR_LINK_OUTPUT_HARDWARE_ERROR,
    };
    CHECK(application_message_engine_note_output_result(
        &engine, &accepted) == NOSTOS_OK);
    CHECK(application_message_engine_note_output_result(
        &engine, &hardware_error) == NOSTOS_IO_ERROR);
    CHECK(engine.stats.uart_tx_ok == 1U);
    CHECK(engine.stats.uart_tx_failed == 1U);
    CHECK(engine.stats.output_result_accepted == 1U);
    CHECK(engine.stats.output_result_hardware_error == 1U);
}

int main(void)
{
    check_local_and_remote_same_engine();
    check_priority_fall_stop_buttons();
    check_hello_ready_and_state_resync();
    check_multi_source_fall_aggregate();
    check_command_id_and_result_counters();
    puts("PASS local and authenticated remote packets share one ESP receiver");
    puts("PASS duplicate remote packets create no second OUTPUT command");
    puts("PASS output priority is FALL then STOP then button");
    puts("PASS HELLO READY resync mirrors active FALL and sensor cache");
    puts("PASS one source clear preserves another source active FALL output");
    puts("PASS command_id wrap fails closed and result counters stay separate");
    return 0;
}
