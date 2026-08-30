#include "application_message_engine.h"
#include "application_event_heap.h"
#include "check.h"
#include "official_packet_writer.h"
#include "output_command_retry.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t command_epoch;
    uint32_t last_command_id;
    uint32_t hardware_actuation_count;
    bool ready;
} stm_output_fixture_t;

typedef struct {
    uint32_t ready_tx_success;
    uint32_t mesh_tx_success;
    uint32_t receive_accept_success;
    uint32_t uart_output_tx_success;
    uint32_t hardware_output_success;
    uint32_t duplicate_output_result;
    uint32_t generated_output_count;
} scenario_counters_t;

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

static size_t encode_official_message(
    const nostos_message_t *message,
    uint8_t wire[NOSTOS_WIRE_MAX])
{
    size_t length = 0U;
    CHECK(nostos_message_encode(
        message, wire, NOSTOS_WIRE_MAX, &length) == NOSTOS_OK);
    return length;
}

static uint32_t output_command_id(const sensor_link_message_t *message)
{
    CHECK(message != NULL);
    switch (message->type) {
    case SENSOR_LINK_OUTPUT_EVENT:
        return message->output_event.command_id;
    case SENSOR_LINK_OUTPUT_RIDE:
        return message->output_ride.command_id;
    case SENSOR_LINK_OUTPUT_ENVIRONMENT:
        return message->output_environment.command_id;
    default:
        CHECK(false);
        return 0U;
    }
}

static void stm_fixture_reboot(stm_output_fixture_t *fixture)
{
    CHECK(fixture != NULL);
    const uint32_t hardware_actuation_count =
        fixture->hardware_actuation_count;
    *fixture = (stm_output_fixture_t){
        .hardware_actuation_count = hardware_actuation_count,
    };
}

static void stm_fixture_receive_ready(
    stm_output_fixture_t *fixture,
    const uint8_t *frame,
    size_t length)
{
    CHECK(fixture != NULL);
    const sensor_link_message_t message = decode_sensor_frame(frame, length);
    CHECK(message.type == SENSOR_LINK_READY);
    CHECK(message.ready.command_epoch != 0U);
    if (!fixture->ready ||
        fixture->command_epoch != message.ready.command_epoch) {
        fixture->command_epoch = message.ready.command_epoch;
        fixture->last_command_id = 0U;
    }
    fixture->ready = true;
}

static sensor_link_output_result_t stm_fixture_receive_output(
    stm_output_fixture_t *fixture,
    const uint8_t *frame,
    size_t length)
{
    CHECK(fixture != NULL);
    CHECK(fixture->ready);
    const sensor_link_message_t output = decode_sensor_frame(frame, length);
    const uint32_t command_id = output_command_id(&output);
    uint8_t status = SENSOR_LINK_OUTPUT_DUPLICATE;
    if (command_id > fixture->last_command_id) {
        fixture->last_command_id = command_id;
        ++fixture->hardware_actuation_count;
        status = SENSOR_LINK_OUTPUT_ACCEPTED;
    }

    uint8_t result_frame[SENSOR_LINK_FRAME_SIZE];
    size_t result_length = 0U;
    CHECK(sensor_link_encode_output_result(
        command_id, status, result_frame, &result_length) == SENSOR_LINK_OK);
    const sensor_link_message_t result = decode_sensor_frame(
        result_frame, result_length);
    CHECK(result.type == SENSOR_LINK_OUTPUT_RESULT);
    return result.output_result;
}

static uint8_t deliver_output_to_stm(
    application_message_engine_t *engine,
    stm_output_fixture_t *fixture,
    scenario_counters_t *counters,
    const uint8_t *frame,
    size_t length)
{
    CHECK(engine != NULL);
    CHECK(counters != NULL);
    ++counters->uart_output_tx_success;
    application_message_engine_note_uart_tx(engine, true);
    const sensor_link_output_result_t result = stm_fixture_receive_output(
        fixture, frame, length);
    if (result.status == SENSOR_LINK_OUTPUT_ACCEPTED) {
        CHECK(application_message_engine_note_output_result(
            engine, &result) == NOSTOS_OK);
        ++counters->hardware_output_success;
    } else {
        CHECK(result.status == SENSOR_LINK_OUTPUT_DUPLICATE);
        CHECK(application_message_engine_note_output_result(
            engine, &result) == NOSTOS_DUPLICATE);
        ++counters->duplicate_output_result;
    }
    return result.status;
}

static void event_heap_expect(
    application_event_heap_t *heap,
    uint32_t now_ms,
    uint8_t expected_type,
    uint16_t expected_sequence)
{
    nostos_job_t job;
    CHECK(application_event_heap_pop(heap, now_ms, &job) == NOSTOS_OK);
    nostos_message_t message;
    CHECK(nostos_message_decode(
        job.wire, job.length, &message) == NOSTOS_OK);
    CHECK(message.type == expected_type);
    CHECK(message.sequence == expected_sequence);
}

int main(void)
{
    enum {
        LOCAL_SOURCE = 1U,
        REMOTE_RIDE_SOURCE = 2U,
        REMOTE_ENV_SOURCE = 3U,
    };
    const uint32_t first_epoch = UINT32_C(0xA11CE001);
    const uint32_t second_epoch = UINT32_C(0xA11CE002);
    const uint32_t ride_session = UINT32_C(2002);
    const uint32_t environment_session = UINT32_C(3003);
    scenario_counters_t counters = {0};
    stm_output_fixture_t stm = {0};
    application_message_engine_t engine;
    official_packet_writer_t local_writer;
    CHECK(application_message_engine_init(
        &engine, LOCAL_SOURCE, first_epoch) == NOSTOS_OK);
    CHECK(official_packet_writer_init(
        &local_writer, LOCAL_SOURCE, first_epoch) == NOSTOS_OK);

    /* STM HELLO -> ESP READY. BTN4 is deliberately absent from this UART/Mesh
     * scenario: it remains a local-only STM action and has no wire event. */
    uint32_t btn4_uart_or_mesh_count = 0U;
    uint8_t sensor_frame[SENSOR_LINK_FRAME_SIZE];
    size_t sensor_length = 0U;
    CHECK(sensor_link_encode_hello(
        sensor_frame, &sensor_length) == SENSOR_LINK_OK);
    CHECK(decode_sensor_frame(
        sensor_frame, sensor_length).type == SENSOR_LINK_HELLO);
    CHECK(sensor_link_encode_ready(
        first_epoch, sensor_frame, &sensor_length) == SENSOR_LINK_OK);
    stm_fixture_receive_ready(&stm, sensor_frame, sensor_length);
    ++counters.ready_tx_success;
    CHECK(stm.command_epoch == first_epoch);
    CHECK(stm.last_command_id == 0U);

    /* BTN1 physical meaning is exactly Pace Up / 0x11. The ESP stamps the
     * sole official packet, accepts it through the common receiver, mirrors it
     * locally, and hands the same official wire to Mesh. */
    CHECK(sensor_link_encode_event(
        SENSOR_LINK_EVENT_SPEED_UP,
        sensor_frame, &sensor_length) == SENSOR_LINK_OK);
    sensor_link_message_t physical = decode_sensor_frame(
        sensor_frame, sensor_length);
    CHECK(physical.type == SENSOR_LINK_EVENT);
    CHECK(physical.event.type == 0x11U);
    CHECK(physical.event.type == NOSTOS_SPEED_UP);

    uint8_t mesh_wire[NOSTOS_WIRE_MAX];
    size_t mesh_length = 0U;
    CHECK(official_packet_writer_event(
        &local_writer, physical.event.type,
        mesh_wire, &mesh_length) == NOSTOS_OK);
    ++counters.mesh_tx_success;
    nostos_message_t mesh_message;
    CHECK(nostos_message_decode(
        mesh_wire, mesh_length, &mesh_message) == NOSTOS_OK);
    CHECK(mesh_message.type == NOSTOS_SPEED_UP);
    CHECK(mesh_message.source_id == LOCAL_SOURCE);
    CHECK(mesh_message.session_id == first_epoch);
    CHECK(mesh_message.sequence == 0U);

    nostos_message_t accepted;
    CHECK(application_message_engine_accept_wire(
        &engine, mesh_wire, mesh_length, 10U, &accepted) == NOSTOS_OK);
    ++counters.receive_accept_success;
    CHECK(accepted.type == NOSTOS_SPEED_UP);
    uint8_t output_frame[SENSOR_LINK_FRAME_SIZE];
    size_t output_length = 0U;
    uint32_t command_id = 0U;
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output_frame, &output_length,
        &command_id) == NOSTOS_OK);
    ++counters.generated_output_count;
    CHECK(command_id == 1U);
    sensor_link_message_t output = decode_sensor_frame(
        output_frame, output_length);
    CHECK(output.type == SENSOR_LINK_OUTPUT_EVENT);
    CHECK(output.output_event.source_id == LOCAL_SOURCE);
    CHECK(output.output_event.event_type == SENSOR_LINK_EVENT_SPEED_UP);
    uint8_t first_output_frame[SENSOR_LINK_FRAME_SIZE];
    const size_t first_output_length = output_length;
    memcpy(first_output_frame, output_frame, output_length);

    const uint32_t before_first_output = stm.hardware_actuation_count;
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters, output_frame, output_length) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);
    CHECK(stm.hardware_actuation_count == before_first_output + 1U);
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters, output_frame, output_length) ==
        SENSOR_LINK_OUTPUT_DUPLICATE);
    CHECK(stm.hardware_actuation_count == before_first_output + 1U);

    /* If UART completion is uncertain, the exact frame/id is retained. It is
     * gated by READY, delayed between attempts, and cleared only on success;
     * no later command_id can overtake it in the production worker. */
    output_command_retry_t output_retry;
    output_command_retry_init(&output_retry);
    CHECK(output_command_retry_store(
        &output_retry, first_output_frame, first_output_length, 1U,
        first_epoch, NOSTOS_SPEED_UP, LOCAL_SOURCE, 100U) == NOSTOS_OK);
    uint8_t retry_frame[SENSOR_LINK_FRAME_SIZE] = {0};
    size_t retry_length = 0U;
    uint32_t retry_command_id = 0U;
    uint8_t retry_type = 0U;
    uint8_t retry_source = 0U;
    CHECK(output_command_retry_peek(
        &output_retry, false, 200U, retry_frame, &retry_length,
        &retry_command_id, &retry_type, &retry_source) == NOSTOS_NOT_READY);
    CHECK(output_command_retry_peek(
        &output_retry, true, 199U, retry_frame, &retry_length,
        &retry_command_id, &retry_type, &retry_source) == NOSTOS_NOT_READY);
    CHECK(output_command_retry_peek(
        &output_retry, true, 200U, retry_frame, &retry_length,
        &retry_command_id, &retry_type, &retry_source) == NOSTOS_OK);
    CHECK(retry_length == first_output_length);
    CHECK(memcmp(retry_frame, first_output_frame, retry_length) == 0);
    CHECK(retry_command_id == 1U && retry_type == NOSTOS_SPEED_UP &&
          retry_source == LOCAL_SOURCE);
    CHECK(output_command_retry_finish(
        &output_retry, retry_command_id, false, 200U) == NOSTOS_OK);
    CHECK(output_command_retry_peek(
        &output_retry, true, 299U, retry_frame, &retry_length,
        &retry_command_id, &retry_type, &retry_source) == NOSTOS_NOT_READY);
    CHECK(output_command_retry_peek(
        &output_retry, true, 300U, retry_frame, &retry_length,
        &retry_command_id, &retry_type, &retry_source) == NOSTOS_OK);
    CHECK(output_command_retry_finish(
        &output_retry, retry_command_id, true, 300U) == NOSTOS_OK);
    CHECK(!output_command_retry_pending(&output_retry));
    CHECK(output_command_retry_store(
        &output_retry, first_output_frame, first_output_length, 1U,
        first_epoch, NOSTOS_SPEED_UP, REMOTE_RIDE_SOURCE, 300U) == NOSTOS_OK);
    CHECK(output_command_retry_discard_source_before_session(
        &output_retry, REMOTE_RIDE_SOURCE, first_epoch + 1U));
    CHECK(!output_command_retry_pending(&output_retry));

    /* A remote packet is accepted once. Its exact replay and a sequence below
     * the authenticated floor allocate no second local output command. */
    CHECK(application_message_engine_approve_authenticated_session(
        &engine, REMOTE_RIDE_SOURCE, ride_session, 100U) == NOSTOS_OK);
    nostos_message_t remote_button = {
        .type = NOSTOS_SPEED_DOWN,
        .source_id = REMOTE_RIDE_SOURCE,
        .session_id = ride_session,
        .sequence = 100U,
    };
    uint8_t remote_wire[NOSTOS_WIRE_MAX];
    size_t remote_length = encode_official_message(
        &remote_button, remote_wire);
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 20U, &accepted) == NOSTOS_OK);
    ++counters.receive_accept_success;
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output_frame, &output_length,
        &command_id) == NOSTOS_OK);
    ++counters.generated_output_count;
    CHECK(command_id == 2U);
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters, output_frame, output_length) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);

    const uint32_t before_rejected_packets = counters.generated_output_count;
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 21U,
        &accepted) == NOSTOS_DUPLICATE);
    remote_button.sequence = 99U;
    remote_length = encode_official_message(&remote_button, remote_wire);
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 22U,
        &accepted) == NOSTOS_STALE);
    CHECK(counters.generated_output_count == before_rejected_packets);
    CHECK(engine.next_command_id == 3U);

    /* id=1 is now lower than last_command_id=2 in the same epoch. */
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters,
        first_output_frame, first_output_length) ==
        SENSOR_LINK_OUTPUT_DUPLICATE);

    /* The production fixed min-heap orders P1 FALL/CLEAR, P2 STOP,
     * P3 BTN2/SPEED_DOWN, P4 BTN1/SPEED_UP. The two P2 STOPs also prove FIFO
     * stability inside one priority. */
    application_event_heap_t event_heap;
    application_event_heap_init(&event_heap);
    uint8_t queued_up[NOSTOS_WIRE_MAX];
    size_t queued_up_length = 0U;
    CHECK(official_packet_writer_event(
        &local_writer, NOSTOS_SPEED_UP,
        queued_up, &queued_up_length) == NOSTOS_OK);
    uint8_t queued_down[NOSTOS_WIRE_MAX];
    size_t queued_down_length = 0U;
    CHECK(official_packet_writer_event(
        &local_writer, NOSTOS_SPEED_DOWN,
        queued_down, &queued_down_length) == NOSTOS_OK);
    uint8_t queued_stop[NOSTOS_WIRE_MAX];
    size_t queued_stop_length = 0U;
    CHECK(official_packet_writer_event(
        &local_writer, NOSTOS_STOP,
        queued_stop, &queued_stop_length) == NOSTOS_OK);
    uint8_t queued_fall[NOSTOS_WIRE_MAX];
    size_t queued_fall_length = 0U;
    CHECK(official_packet_writer_event(
        &local_writer, NOSTOS_FALL,
        queued_fall, &queued_fall_length) == NOSTOS_OK);
    uint8_t queued_stop_second[NOSTOS_WIRE_MAX];
    size_t queued_stop_second_length = 0U;
    CHECK(official_packet_writer_event(
        &local_writer, NOSTOS_STOP,
        queued_stop_second, &queued_stop_second_length) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        &event_heap, queued_up, queued_up_length, 100U) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        &event_heap, queued_stop, queued_stop_length, 101U) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        &event_heap, queued_down, queued_down_length, 102U) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        &event_heap, queued_fall, queued_fall_length, 103U) == NOSTOS_OK);
    CHECK(application_event_heap_push(
        &event_heap, queued_stop_second,
        queued_stop_second_length, 104U) == NOSTOS_OK);
    CHECK(application_event_heap_priority(&event_heap) == 1U);
    event_heap_expect(&event_heap, 105U, NOSTOS_FALL, 4U);
    CHECK(application_event_heap_priority(&event_heap) == 2U);
    event_heap_expect(&event_heap, 105U, NOSTOS_STOP, 3U);
    CHECK(application_event_heap_priority(&event_heap) == 2U);
    event_heap_expect(&event_heap, 105U, NOSTOS_STOP, 5U);
    CHECK(application_event_heap_priority(&event_heap) == 3U);
    event_heap_expect(&event_heap, 105U, NOSTOS_SPEED_DOWN, 2U);
    CHECK(application_event_heap_priority(&event_heap) == 4U);
    event_heap_expect(&event_heap, 105U, NOSTOS_SPEED_UP, 1U);
    CHECK(application_event_heap_priority(&event_heap) == 0U);
    CHECK(event_heap.count == 0U);

    /* Different authenticated sources publish RIDE and ENVIRONMENT. The ESP
     * receiver and the local mirror both retain their origin and values. */
    CHECK(application_message_engine_approve_authenticated_session(
        &engine, REMOTE_ENV_SOURCE, environment_session, 200U) == NOSTOS_OK);
    const nostos_message_t ride = {
        .type = NOSTOS_RIDE,
        .source_id = REMOTE_RIDE_SOURCE,
        .session_id = ride_session,
        .sequence = 101U,
        .payload.ride = {
            .valid = true,
            .kmh_x10 = 273U,
            .distance_mm = 123456U,
        },
    };
    remote_length = encode_official_message(&ride, remote_wire);
    /* Sensors use the engine's latest per-source state slots, never the event
     * heap that is reserved for actuator events. */
    CHECK(application_event_heap_push(
        &event_heap, remote_wire, remote_length, 199U) ==
        NOSTOS_UNSUPPORTED_TYPE);
    CHECK(event_heap.count == 0U);
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 200U, &accepted) == NOSTOS_OK);
    ++counters.receive_accept_success;
    CHECK(accepted.source_id == REMOTE_RIDE_SOURCE);
    CHECK(accepted.session_id == ride_session);
    CHECK(accepted.sequence == 101U);
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output_frame, &output_length,
        &command_id) == NOSTOS_OK);
    ++counters.generated_output_count;
    output = decode_sensor_frame(output_frame, output_length);
    CHECK(output.type == SENSOR_LINK_OUTPUT_RIDE);
    CHECK(output.output_ride.source_id == REMOTE_RIDE_SOURCE);
    CHECK(output.output_ride.kmh_x10 == 273U);
    CHECK(output.output_ride.distance_mm == 123456U);
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters, output_frame, output_length) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);

    const nostos_message_t environment = {
        .type = NOSTOS_ENVIRONMENT,
        .source_id = REMOTE_ENV_SOURCE,
        .session_id = environment_session,
        .sequence = 200U,
        .payload.environment = {
            .temperature_c_x10 = 245,
            .humidity_pct_x10 = 550U,
            .temperature_quality = NOSTOS_VALID,
            .humidity_quality = NOSTOS_VALID,
        },
    };
    remote_length = encode_official_message(&environment, remote_wire);
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 201U, &accepted) == NOSTOS_OK);
    ++counters.receive_accept_success;
    CHECK(accepted.source_id == REMOTE_ENV_SOURCE);
    CHECK(accepted.session_id == environment_session);
    CHECK(accepted.sequence == 200U);
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output_frame, &output_length,
        &command_id) == NOSTOS_OK);
    ++counters.generated_output_count;
    output = decode_sensor_frame(output_frame, output_length);
    CHECK(output.type == SENSOR_LINK_OUTPUT_ENVIRONMENT);
    CHECK(output.output_environment.source_id == REMOTE_ENV_SOURCE);
    CHECK(output.output_environment.temperature_c_x10 == 245);
    CHECK(output.output_environment.humidity_pct_x10 == 550U);
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters, output_frame, output_length) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);

    /* Two independent FALL incidents aggregate. Clearing source 2 keeps the
     * source 3 FALL output active; it is not a global clear. */
    const nostos_message_t fall_ride_source = {
        .type = NOSTOS_FALL,
        .source_id = REMOTE_RIDE_SOURCE,
        .session_id = ride_session,
        .sequence = 102U,
        .payload.incident = {
            .session_id = ride_session,
            .incident_id = 1U,
        },
    };
    const nostos_message_t fall_environment_source = {
        .type = NOSTOS_FALL,
        .source_id = REMOTE_ENV_SOURCE,
        .session_id = environment_session,
        .sequence = 201U,
        .payload.incident = {
            .session_id = environment_session,
            .incident_id = 1U,
        },
    };
    const nostos_message_t clear_ride_source = {
        .type = NOSTOS_FALL_CLEAR,
        .source_id = REMOTE_RIDE_SOURCE,
        .session_id = ride_session,
        .sequence = 103U,
        .payload.incident = {
            .session_id = ride_session,
            .incident_id = 1U,
        },
    };
    const nostos_message_t clear_environment_source = {
        .type = NOSTOS_FALL_CLEAR,
        .source_id = REMOTE_ENV_SOURCE,
        .session_id = environment_session,
        .sequence = 202U,
        .payload.incident = {
            .session_id = environment_session,
            .incident_id = 1U,
        },
    };
    const nostos_message_t incident_messages[] = {
        fall_ride_source,
        fall_environment_source,
        clear_ride_source,
    };
    for (size_t i = 0U;
         i < sizeof(incident_messages) / sizeof(incident_messages[0]);
         ++i) {
        remote_length = encode_official_message(
            &incident_messages[i], remote_wire);
        CHECK(application_message_engine_accept_wire(
            &engine, remote_wire, remote_length,
            300U + (uint32_t)i, &accepted) == NOSTOS_OK);
        ++counters.receive_accept_success;
        CHECK(application_message_engine_encode_output(
            &engine, &accepted, output_frame, &output_length,
            &command_id) == NOSTOS_OK);
        ++counters.generated_output_count;
        output = decode_sensor_frame(output_frame, output_length);
        CHECK(output.type == SENSOR_LINK_OUTPUT_EVENT);
        CHECK(output.output_event.event_type == SENSOR_LINK_EVENT_FALL);
        if (i == 2U) {
            CHECK(output.output_event.source_id == REMOTE_ENV_SOURCE);
        }
        CHECK(deliver_output_to_stm(
            &engine, &stm, &counters, output_frame, output_length) ==
            SENSOR_LINK_OUTPUT_ACCEPTED);
    }
    nostos_outputs_t aggregate = nostos_receiver_outputs(
        &engine.receiver, 303U);
    CHECK(aggregate.led == NOSTOS_LED_RED_BLINK);
    CHECK(aggregate.buzzer == NOSTOS_BUZZER_EMERGENCY);

    /* The paired STM reboots, says HELLO, and receives READY followed by the
     * active FALL and every fresh per-source sensor snapshot. READY uses the
     * same nonzero ESP boot epoch; all resync OUTPUTs carry command_id. */
    stm_fixture_reboot(&stm);
    CHECK(sensor_link_encode_hello(
        sensor_frame, &sensor_length) == SENSOR_LINK_OK);
    CHECK(decode_sensor_frame(
        sensor_frame, sensor_length).type == SENSOR_LINK_HELLO);
    CHECK(sensor_link_encode_ready(
        first_epoch, sensor_frame, &sensor_length) == SENSOR_LINK_OK);
    stm_fixture_receive_ready(&stm, sensor_frame, sensor_length);
    ++counters.ready_tx_success;
    CHECK(stm.command_epoch == first_epoch);

    nostos_message_t snapshots[APPLICATION_MESSAGE_SNAPSHOT_CAPACITY];
    size_t snapshot_count = 0U;
    CHECK(application_message_engine_snapshot(
        &engine, 304U, snapshots, &snapshot_count) == NOSTOS_OK);
    CHECK(snapshot_count == 3U);
    CHECK(snapshots[0].type == NOSTOS_FALL);
    CHECK(snapshots[0].source_id == REMOTE_ENV_SOURCE);
    CHECK(snapshots[0].session_id == environment_session);
    CHECK(snapshots[0].sequence == 201U);
    CHECK(snapshots[1].type == NOSTOS_RIDE);
    CHECK(snapshots[1].source_id == REMOTE_RIDE_SOURCE);
    CHECK(snapshots[1].session_id == ride_session);
    CHECK(snapshots[1].sequence == 101U);
    CHECK(snapshots[2].type == NOSTOS_ENVIRONMENT);
    CHECK(snapshots[2].source_id == REMOTE_ENV_SOURCE);
    CHECK(snapshots[2].session_id == environment_session);
    CHECK(snapshots[2].sequence == 200U);
    uint32_t previous_command_id = 0U;
    for (size_t i = 0U; i < snapshot_count; ++i) {
        CHECK(application_message_engine_encode_output(
            &engine, &snapshots[i], output_frame, &output_length,
            &command_id) == NOSTOS_OK);
        ++counters.generated_output_count;
        CHECK(command_id != 0U);
        CHECK(command_id > previous_command_id);
        previous_command_id = command_id;
        CHECK(deliver_output_to_stm(
            &engine, &stm, &counters, output_frame, output_length) ==
            SENSOR_LINK_OUTPUT_ACCEPTED);
    }

    remote_length = encode_official_message(
        &clear_environment_source, remote_wire);
    CHECK(application_message_engine_accept_wire(
        &engine, remote_wire, remote_length, 305U,
        &accepted) == NOSTOS_OK);
    ++counters.receive_accept_success;
    CHECK(application_message_engine_encode_output(
        &engine, &accepted, output_frame, &output_length,
        &command_id) == NOSTOS_OK);
    ++counters.generated_output_count;
    output = decode_sensor_frame(output_frame, output_length);
    CHECK(output.type == SENSOR_LINK_OUTPUT_EVENT);
    CHECK(output.output_event.event_type == SENSOR_LINK_EVENT_FALL_CLEAR);
    CHECK(deliver_output_to_stm(
        &engine, &stm, &counters, output_frame, output_length) ==
        SENSOR_LINK_OUTPUT_ACCEPTED);
    aggregate = nostos_receiver_outputs(&engine.receiver, 306U);
    CHECK(aggregate.led == NOSTOS_LED_OFF);
    CHECK(aggregate.buzzer == NOSTOS_BUZZER_OFF);

    /* A new ESP process uses a new epoch and restarts command_id at one. STM
     * must treat id=1 as new only after that newer READY epoch. */
    application_message_engine_t restarted_engine;
    official_packet_writer_t restarted_writer;
    CHECK(application_message_engine_init(
        &restarted_engine, LOCAL_SOURCE, second_epoch) == NOSTOS_OK);
    CHECK(official_packet_writer_init(
        &restarted_writer, LOCAL_SOURCE, second_epoch) == NOSTOS_OK);
    CHECK(sensor_link_encode_ready(
        second_epoch, sensor_frame, &sensor_length) == SENSOR_LINK_OK);
    stm_fixture_receive_ready(&stm, sensor_frame, sensor_length);
    ++counters.ready_tx_success;
    CHECK(stm.command_epoch == second_epoch);
    CHECK(stm.last_command_id == 0U);
    CHECK(official_packet_writer_event(
        &restarted_writer, NOSTOS_STOP,
        mesh_wire, &mesh_length) == NOSTOS_OK);
    ++counters.mesh_tx_success;
    CHECK(application_message_engine_accept_wire(
        &restarted_engine, mesh_wire, mesh_length, 400U,
        &accepted) == NOSTOS_OK);
    ++counters.receive_accept_success;
    CHECK(application_message_engine_encode_output(
        &restarted_engine, &accepted, output_frame, &output_length,
        &command_id) == NOSTOS_OK);
    ++counters.generated_output_count;
    CHECK(command_id == 1U);
    CHECK(deliver_output_to_stm(
        &restarted_engine, &stm, &counters,
        output_frame, output_length) == SENSOR_LINK_OUTPUT_ACCEPTED);

    CHECK(btn4_uart_or_mesh_count == 0U);
    CHECK(counters.ready_tx_success == 3U);
    CHECK(counters.mesh_tx_success == 2U);
    CHECK(counters.receive_accept_success == 9U);
    CHECK(counters.generated_output_count == 12U);
    CHECK(counters.hardware_output_success == 12U);
    CHECK(counters.duplicate_output_result == 2U);
    CHECK(counters.uart_output_tx_success == 14U);
    CHECK(stm.hardware_actuation_count ==
        counters.hardware_output_success);
    CHECK(engine.stats.accepted == 8U);
    CHECK(engine.stats.duplicate == 1U);
    CHECK(engine.stats.rejected == 1U);
    CHECK(engine.stats.uart_tx_ok == 13U);
    CHECK(engine.stats.output_result_accepted == 11U);
    CHECK(engine.stats.output_result_duplicate == 2U);
    CHECK(restarted_engine.stats.accepted == 1U);
    CHECK(restarted_engine.stats.uart_tx_ok == 1U);
    CHECK(restarted_engine.stats.output_result_accepted == 1U);

    puts("NOSTOS_PROJECT_COMPLEX_SCENARIO PASS");
    return 0;
}
