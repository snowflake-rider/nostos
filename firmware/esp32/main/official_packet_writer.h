#ifndef NOSTOS_OFFICIAL_PACKET_WRITER_H
#define NOSTOS_OFFICIAL_PACKET_WRITER_H

#include "nostos_protocol.h"

typedef struct {
    nostos_sender_t sender;
    nostos_incident_ref_t active_fall;
    uint32_t next_incident_id;
    bool fall_active;
    bool initialized;
} official_packet_writer_t;

nostos_result_t official_packet_writer_init(
    official_packet_writer_t *writer,
    uint8_t source_id,
    uint32_t session_id);

/* A Mesh identity remap changes only source ownership. The boot session and
 * monotonic sequence remain owned by this ESP process. */
nostos_result_t official_packet_writer_set_source(
    official_packet_writer_t *writer,
    uint8_t source_id);

nostos_result_t official_packet_writer_event(
    official_packet_writer_t *writer,
    uint8_t type,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length);

nostos_result_t official_packet_writer_ride(
    official_packet_writer_t *writer,
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length);

nostos_result_t official_packet_writer_environment(
    official_packet_writer_t *writer,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    nostos_quality_t temperature_quality,
    nostos_quality_t humidity_quality,
    uint8_t wire[NOSTOS_WIRE_MAX],
    size_t *length);

#endif
