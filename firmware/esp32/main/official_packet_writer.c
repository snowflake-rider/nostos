#include "official_packet_writer.h"

#include <limits.h>

static nostos_result_t stamp_and_encode(
    official_packet_writer_t *writer,
    nostos_message_t *message,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length)
{
    if (writer == NULL || message == NULL || wire == NULL || length == NULL ||
        !writer->initialized) {
        return NOSTOS_BAD_ARGUMENT;
    }
    nostos_result_t result = nostos_sender_stamp(&writer->sender, message);
    if (result != NOSTOS_OK) return result;
    return nostos_message_encode(message, wire, NOSTOS_WIRE_MAX, length);
}

nostos_result_t official_packet_writer_init(
    official_packet_writer_t *writer,
    uint8_t source_id,
    uint32_t session_id)
{
    if (writer == NULL) return NOSTOS_BAD_ARGUMENT;
    nostos_sender_t sender;
    nostos_result_t result = nostos_sender_init(
        &sender, source_id, session_id);
    if (result != NOSTOS_OK) return result;
    *writer = (official_packet_writer_t){
        .sender = sender,
        .next_incident_id = 1U,
        .initialized = true,
    };
    return NOSTOS_OK;
}

nostos_result_t official_packet_writer_set_source(
    official_packet_writer_t *writer,
    uint8_t source_id)
{
    if (writer == NULL || !writer->initialized || source_id < 1U ||
        source_id > NOSTOS_NODE_COUNT) {
        return NOSTOS_BAD_ARGUMENT;
    }
    writer->sender.source_id = source_id;
    writer->fall_active = false;
    writer->active_fall = (nostos_incident_ref_t){0};
    return NOSTOS_OK;
}

nostos_result_t official_packet_writer_event(
    official_packet_writer_t *writer,
    uint8_t type,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length)
{
    if (writer == NULL || !writer->initialized) return NOSTOS_BAD_ARGUMENT;
    nostos_message_t message = {.type = type};
    nostos_incident_ref_t incident = {0};
    if (type == NOSTOS_FALL) {
        if (writer->fall_active) {
            incident = writer->active_fall;
        } else {
            if (writer->next_incident_id == 0U ||
                writer->next_incident_id > UINT16_MAX) {
                return NOSTOS_EXHAUSTED;
            }
            incident = (nostos_incident_ref_t){
                .session_id = writer->sender.session_id,
                .incident_id = (uint16_t)writer->next_incident_id,
            };
        }
        message.payload.incident = incident;
    } else if (type == NOSTOS_FALL_CLEAR) {
        if (!writer->fall_active) return NOSTOS_STALE;
        incident = writer->active_fall;
        message.payload.incident = incident;
    } else if (type != NOSTOS_SPEED_DOWN && type != NOSTOS_SPEED_UP &&
               type != NOSTOS_STOP) {
        return NOSTOS_BAD_VALUE;
    }

    nostos_result_t result = stamp_and_encode(writer, &message, wire, length);
    if (result != NOSTOS_OK) return result;
    if (type == NOSTOS_FALL) {
        bool new_incident = !writer->fall_active;
        writer->active_fall = incident;
        writer->fall_active = true;
        if (new_incident) ++writer->next_incident_id;
    } else if (type == NOSTOS_FALL_CLEAR) {
        writer->fall_active = false;
    }
    return NOSTOS_OK;
}

nostos_result_t official_packet_writer_ride(
    official_packet_writer_t *writer,
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length)
{
    nostos_message_t message = {
        .type = NOSTOS_RIDE,
        .payload.ride = {
            .valid = valid,
            .kmh_x10 = valid ? kmh_x10 : 0U,
            .distance_mm = valid ? distance_mm : 0U,
        },
    };
    if (!valid && (kmh_x10 != 0U || distance_mm != 0U)) {
        return NOSTOS_BAD_VALUE;
    }
    return stamp_and_encode(writer, &message, wire, length);
}

nostos_result_t official_packet_writer_environment(
    official_packet_writer_t *writer,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    nostos_quality_t temperature_quality,
    nostos_quality_t humidity_quality,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length)
{
    nostos_message_t message = {
        .type = NOSTOS_ENVIRONMENT,
        .payload.environment = {
            .temperature_c_x10 = temperature_c_x10,
            .humidity_pct_x10 = humidity_pct_x10,
            .temperature_quality = temperature_quality,
            .humidity_quality = humidity_quality,
        },
    };
    return stamp_and_encode(writer, &message, wire, length);
}
