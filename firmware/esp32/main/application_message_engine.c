#include "application_message_engine.h"

#include <limits.h>

static bool output_event_type(uint8_t type)
{
    return type == NOSTOS_SPEED_DOWN || type == NOSTOS_SPEED_UP ||
        type == NOSTOS_STOP || type == NOSTOS_FALL ||
        type == NOSTOS_FALL_CLEAR;
}

nostos_result_t application_message_engine_init(
    application_message_engine_t *engine,
    uint8_t local_source,
    uint32_t local_session)
{
    if (engine == NULL || local_session == 0U) return NOSTOS_BAD_ARGUMENT;
    application_message_engine_t initialized = {
        .next_command_id = 1U,
        .local_source = local_source,
        .initialized = true,
    };
    nostos_result_t result = nostos_receiver_init(
        &initialized.receiver, local_source);
    if (result != NOSTOS_OK) return result;
    result = nostos_receiver_approve_session(
        &initialized.receiver, local_source, local_session, 0U);
    if (result != NOSTOS_OK) return result;
    shared_data_cache_init(&initialized.cache);
    *engine = initialized;
    return NOSTOS_OK;
}

nostos_result_t application_message_engine_approve_authenticated_session(
    application_message_engine_t *engine,
    uint8_t source_id,
    uint32_t session_id,
    uint16_t sequence_floor)
{
    if (engine == NULL || !engine->initialized || session_id == 0U ||
        source_id < 1U || source_id > NOSTOS_NODE_COUNT) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (source_id == engine->local_source) return NOSTOS_UNAUTHORIZED;
    const nostos_rx_window_t *window = &engine->receiver.windows[source_id - 1U];
    if (window->approved) {
        if (session_id == window->session_id) return NOSTOS_OK;
        if (session_id < window->session_id) return NOSTOS_STALE;
    }
    return nostos_receiver_approve_session(
        &engine->receiver, source_id, session_id, sequence_floor);
}

nostos_result_t application_message_engine_accept_wire(
    application_message_engine_t *engine,
    const uint8_t *wire,
    size_t length,
    uint32_t now_ms,
    nostos_message_t *accepted_message)
{
    if (engine == NULL || !engine->initialized || wire == NULL ||
        accepted_message == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    nostos_message_t decoded;
    nostos_result_t result = nostos_message_decode(wire, length, &decoded);
    if (result == NOSTOS_OK) {
        result = nostos_receiver_apply(&engine->receiver, &decoded, now_ms);
    }
    if (result != NOSTOS_OK) {
        if (result == NOSTOS_DUPLICATE) {
            ++engine->stats.duplicate;
        } else {
            ++engine->stats.rejected;
        }
        return result;
    }

    if (decoded.type == NOSTOS_RIDE || decoded.type == NOSTOS_ENVIRONMENT) {
        nostos_result_t cached = shared_data_cache_store(
            &engine->cache, wire, length, now_ms);
        /* Receiver acceptance is authoritative. Cache pressure or stale
         * replacement policy must never make one accepted packet appear
         * rejected after its sequence window and state already advanced. */
        (void)cached;
    } else if (decoded.type == NOSTOS_STOP) {
        nostos_message_t consumed;
        (void)nostos_receiver_take_stop(&engine->receiver, &consumed);
    } else if (decoded.type == NOSTOS_SPEED_DOWN ||
               decoded.type == NOSTOS_SPEED_UP) {
        nostos_message_t consumed;
        (void)nostos_receiver_take_button(&engine->receiver, &consumed);
    }

    *accepted_message = decoded;
    ++engine->stats.accepted;
    return NOSTOS_OK;
}

nostos_result_t application_message_engine_capture_output(
    const application_message_engine_t *engine,
    const nostos_message_t *accepted_message,
    nostos_message_t *captured_message)
{
    if (engine == NULL || !engine->initialized || accepted_message == NULL ||
        captured_message == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (!output_event_type(accepted_message->type) &&
        accepted_message->type != NOSTOS_RIDE &&
        accepted_message->type != NOSTOS_ENVIRONMENT) {
        return NOSTOS_UNSUPPORTED_TYPE;
    }
    *captured_message = *accepted_message;
    if (accepted_message->type == NOSTOS_FALL_CLEAR) {
        captured_message->type = NOSTOS_FALL_CLEAR;
        for (size_t i = 0U; i < NOSTOS_INCIDENT_CAPACITY; ++i) {
            const nostos_incident_record_t *incident =
                &engine->receiver.incidents[i];
            if (incident->used && !incident->closed &&
                incident->kind == NOSTOS_FALL) {
                captured_message->type = NOSTOS_FALL;
                captured_message->source_id = incident->source_id;
                captured_message->session_id = incident->ref.session_id;
                captured_message->payload.incident = incident->ref;
                break;
            }
        }
    }
    return NOSTOS_OK;
}

nostos_result_t application_message_engine_encode_captured_output(
    application_message_engine_t *engine,
    const nostos_message_t *captured_message,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length,
    uint32_t *command_id)
{
    if (engine == NULL || !engine->initialized || captured_message == NULL ||
        frame == NULL || frame_length == NULL || command_id == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }
    if (!output_event_type(captured_message->type) &&
        captured_message->type != NOSTOS_RIDE &&
        captured_message->type != NOSTOS_ENVIRONMENT) {
        return NOSTOS_UNSUPPORTED_TYPE;
    }
    if (engine->next_command_id == 0U) return NOSTOS_EXHAUSTED;

    const uint32_t selected_id = engine->next_command_id;
    sensor_link_result_t encoded;
    if (output_event_type(captured_message->type)) {
        encoded = sensor_link_encode_output_event(
            selected_id, captured_message->source_id, captured_message->type,
            frame, frame_length);
    } else if (captured_message->type == NOSTOS_RIDE) {
        encoded = sensor_link_encode_output_ride(
            selected_id, captured_message->source_id,
            captured_message->payload.ride.valid,
            captured_message->payload.ride.kmh_x10,
            captured_message->payload.ride.distance_mm,
            frame, frame_length);
    } else {
        encoded = sensor_link_encode_output_environment(
            selected_id, captured_message->source_id,
            captured_message->payload.environment.temperature_c_x10,
            captured_message->payload.environment.humidity_pct_x10,
            (uint8_t)captured_message->payload.environment.temperature_quality,
            (uint8_t)captured_message->payload.environment.humidity_quality,
            frame, frame_length);
    }
    if (encoded != SENSOR_LINK_OK) return NOSTOS_BAD_VALUE;

    *command_id = selected_id;
    engine->next_command_id = selected_id == UINT32_MAX
        ? 0U : selected_id + 1U;
    return NOSTOS_OK;
}

nostos_result_t application_message_engine_encode_output(
    application_message_engine_t *engine,
    const nostos_message_t *accepted_message,
    uint8_t frame[SENSOR_LINK_FRAME_SIZE],
    size_t *frame_length,
    uint32_t *command_id)
{
    nostos_message_t captured;
    nostos_result_t result = application_message_engine_capture_output(
        engine, accepted_message, &captured);
    return result == NOSTOS_OK
        ? application_message_engine_encode_captured_output(
            engine, &captured, frame, frame_length, command_id)
        : result;
}

nostos_result_t application_message_engine_snapshot(
    const application_message_engine_t *engine,
    uint32_t now_ms,
    nostos_message_t messages[APPLICATION_MESSAGE_SNAPSHOT_CAPACITY],
    size_t *message_count)
{
    if (message_count != NULL) *message_count = 0U;
    if (engine == NULL || !engine->initialized || messages == NULL ||
        message_count == NULL) {
        return NOSTOS_BAD_ARGUMENT;
    }

    for (size_t i = 0U; i < NOSTOS_NODE_COUNT; ++i) {
        const nostos_node_state_t *node = &engine->receiver.shared_data.nodes[i];
        if (node->fall.phase != NOSTOS_INCIDENT_ACTIVE ||
            !node->fall.last_report.seen) {
            continue;
        }
        messages[*message_count] = (nostos_message_t){
            .type = NOSTOS_FALL,
            .source_id = node->source_id,
            .session_id = node->fall.last_report.session_id,
            .sequence = node->fall.last_report.sequence,
            .payload.incident = node->fall.incident,
        };
        ++*message_count;
    }

    for (size_t i = 0U; i < NOSTOS_NODE_COUNT; ++i) {
        const nostos_node_state_t *node = &engine->receiver.shared_data.nodes[i];
        if (nostos_report_fresh(
                &node->ride.report, now_ms, NOSTOS_FRESH_MS)) {
            bool valid = node->ride.speed_kmh_x10.quality == NOSTOS_VALID &&
                node->ride.distance_mm.quality == NOSTOS_VALID &&
                node->ride.speed_kmh_x10.has_value &&
                node->ride.distance_mm.has_value;
            messages[*message_count] = (nostos_message_t){
                .type = NOSTOS_RIDE,
                .source_id = node->source_id,
                .session_id = node->ride.report.session_id,
                .sequence = node->ride.report.sequence,
                .payload.ride = {
                    .valid = valid,
                    .kmh_x10 = valid ? node->ride.speed_kmh_x10.value : 0U,
                    .distance_mm = valid ? node->ride.distance_mm.value : 0U,
                },
            };
            ++*message_count;
        }
        if (nostos_report_fresh(
                &node->environment.report, now_ms, NOSTOS_FRESH_MS)) {
            messages[*message_count] = (nostos_message_t){
                .type = NOSTOS_ENVIRONMENT,
                .source_id = node->source_id,
                .session_id = node->environment.report.session_id,
                .sequence = node->environment.report.sequence,
                .payload.environment = {
                    .temperature_c_x10 =
                        node->environment.temperature_c_x10.has_value
                        ? node->environment.temperature_c_x10.value : 0,
                    .humidity_pct_x10 =
                        node->environment.humidity_pct_x10.has_value
                        ? node->environment.humidity_pct_x10.value : 0U,
                    .temperature_quality =
                        node->environment.temperature_c_x10.quality,
                    .humidity_quality =
                        node->environment.humidity_pct_x10.quality,
                },
            };
            ++*message_count;
        }
    }
    return NOSTOS_OK;
}

void application_message_engine_note_uart_tx(
    application_message_engine_t *engine,
    bool succeeded)
{
    if (engine == NULL || !engine->initialized) return;
    if (succeeded) {
        ++engine->stats.uart_tx_ok;
    } else {
        ++engine->stats.uart_tx_failed;
    }
}

nostos_result_t application_message_engine_note_output_result(
    application_message_engine_t *engine,
    const sensor_link_output_result_t *result)
{
    if (engine == NULL || !engine->initialized || result == NULL ||
        result->command_id == 0U) {
        return NOSTOS_BAD_ARGUMENT;
    }
    switch (result->status) {
    case SENSOR_LINK_OUTPUT_ACCEPTED:
        ++engine->stats.output_result_accepted;
        return NOSTOS_OK;
    case SENSOR_LINK_OUTPUT_DUPLICATE:
        ++engine->stats.output_result_duplicate;
        return NOSTOS_DUPLICATE;
    case SENSOR_LINK_OUTPUT_REJECTED:
        ++engine->stats.output_result_rejected;
        return NOSTOS_UNAUTHORIZED;
    case SENSOR_LINK_OUTPUT_HARDWARE_ERROR:
        ++engine->stats.output_result_hardware_error;
        return NOSTOS_IO_ERROR;
    default:
        ++engine->stats.output_result_unrecognized;
        return NOSTOS_BAD_VALUE;
    }
}
