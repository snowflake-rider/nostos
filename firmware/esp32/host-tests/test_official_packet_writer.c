#include "check.h"
#include "official_packet_writer.h"

#include <stdio.h>
#include <string.h>

static nostos_message_t decode(
    const uint8_t wire[NOSTOS_WIRE_MAX],
    size_t length)
{
    nostos_message_t message = {0};
    CHECK(nostos_message_decode(wire, length, &message) == NOSTOS_OK);
    return message;
}

int main(void)
{
    official_packet_writer_t writer = {0};
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length = 0U;

    CHECK(official_packet_writer_init(&writer, 1U, 77U) == NOSTOS_OK);
    CHECK(official_packet_writer_ride(
        &writer, true, 253U, 123456U, wire, &length) == NOSTOS_OK);
    nostos_message_t message = decode(wire, length);
    CHECK(message.type == NOSTOS_RIDE);
    CHECK(message.source_id == 1U && message.session_id == 77U);
    CHECK(message.sequence == 0U);
    CHECK(message.payload.ride.kmh_x10 == 253U);
    CHECK(message.payload.ride.distance_mm == 123456U);

    CHECK(official_packet_writer_environment(
        &writer, 241, 553U, NOSTOS_VALID, NOSTOS_VALID,
        wire, &length) == NOSTOS_OK);
    message = decode(wire, length);
    CHECK(message.type == NOSTOS_ENVIRONMENT);
    CHECK(message.sequence == 1U);
    CHECK(message.payload.environment.temperature_c_x10 == 241);

    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL_CLEAR, wire, &length) == NOSTOS_STALE);
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL, wire, &length) == NOSTOS_OK);
    message = decode(wire, length);
    CHECK(message.sequence == 2U);
    CHECK(message.payload.incident.session_id == 77U);
    CHECK(message.payload.incident.incident_id == 1U);

    /* Repeated detection refreshes the same active incident. It must not make
     * the eventual CLEAR unable to close the earlier FALL on another node. */
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL, wire, &length) == NOSTOS_OK);
    message = decode(wire, length);
    CHECK(message.sequence == 3U);
    CHECK(message.payload.incident.session_id == 77U);
    CHECK(message.payload.incident.incident_id == 1U);

    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL_CLEAR, wire, &length) == NOSTOS_OK);
    message = decode(wire, length);
    CHECK(message.sequence == 4U);
    CHECK(message.payload.incident.session_id == 77U);
    CHECK(message.payload.incident.incident_id == 1U);
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL_CLEAR, wire, &length) == NOSTOS_STALE);

    CHECK(official_packet_writer_set_source(&writer, 3U) == NOSTOS_OK);
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_STOP, wire, &length) == NOSTOS_OK);
    message = decode(wire, length);
    CHECK(message.source_id == 3U && message.session_id == 77U);
    CHECK(message.sequence == 5U);

    official_packet_writer_t transaction_start = writer;
    uint8_t first_attempt[NOSTOS_WIRE_MAX];
    size_t first_length = 0U;
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL, first_attempt, &first_length) == NOSTOS_OK);
    writer = transaction_start; /* Runtime rollback after transient route failure. */
    CHECK(official_packet_writer_event(
        &writer, NOSTOS_FALL, wire, &length) == NOSTOS_OK);
    CHECK(length == first_length);
    CHECK(memcmp(wire, first_attempt, length) == 0);

    puts("PASS ESP official writer owns one source/session/sequence stream");
    puts("PASS FALL_CLEAR references the active ESP-owned incident");
    puts("PASS writer snapshot rollback regenerates the exact official wire");
    return 0;
}
