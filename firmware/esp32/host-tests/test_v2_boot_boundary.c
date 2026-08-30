#include "check.h"
#include "nostos_bridge.h"
#include "official_packet_writer.h"

#include <stdio.h>

static size_t encode_event(
    official_packet_writer_t *writer,
    uint8_t type,
    uint8_t wire[NOSTOS_WIRE_MAX])
{
    size_t length = 0U;
    CHECK(official_packet_writer_event(
        writer, type, wire, &length) == NOSTOS_OK);
    return length;
}

int main(void)
{
    const nostos_peer_t peers[NOSTOS_NODE_COUNT] = {
        {0x0076U, 1U, 1U},
        {0x00D6U, 2U, 2U},
        {0x00B6U, 3U, 3U},
    };
    nostos_bridge_t bridge;
    official_packet_writer_t writer;
    uint8_t wire[NOSTOS_WIRE_MAX];

    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);
    CHECK(official_packet_writer_init(&writer, 1U, 41U) == NOSTOS_OK);
    size_t length = encode_event(&writer, NOSTOS_SPEED_UP, wire);
    CHECK(nostos_bridge_accept(
        &bridge, NOSTOS_TO_MESH, wire, length, 0U, 10U, true) == NOSTOS_OK);

    /* STM HELLO is only a mirror restart. The ESP writer and Mesh queue are
     * deliberately untouched, so the next packet keeps one sequence stream. */
    length = encode_event(&writer, NOSTOS_STOP, wire);
    CHECK(nostos_bridge_accept(
        &bridge, NOSTOS_TO_MESH, wire, length, 0U, 11U, true) == NOSTOS_OK);
    CHECK(bridge.count == 2U);

    nostos_job_t job;
    nostos_message_t decoded = {0};
    CHECK(nostos_bridge_next(&bridge, 12U, true, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(job.wire, job.length, &decoded) == NOSTOS_OK);
    CHECK(decoded.type == NOSTOS_STOP);
    CHECK(decoded.session_id == 41U && decoded.sequence == 1U);

    CHECK(nostos_bridge_next(&bridge, 13U, true, &job) == NOSTOS_OK);
    CHECK(nostos_message_decode(job.wire, job.length, &decoded) == NOSTOS_OK);
    CHECK(decoded.type == NOSTOS_SPEED_UP);
    CHECK(decoded.session_id == 41U && decoded.sequence == 0U);
    CHECK(nostos_bridge_next(&bridge, 14U, true, &job) == NOSTOS_EMPTY);

    puts("PASS STM mirror reboot preserves ESP writer session and sequence");
    puts("PASS pending Mesh jobs survive an STM-only reboot");
    return 0;
}
