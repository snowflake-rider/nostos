#include "check.h"
#include "nostos_bridge.h"

#include <stdio.h>

static size_t encode_stop(
    uint8_t source,
    uint32_t session,
    uint16_t sequence,
    uint8_t wire[NOSTOS_WIRE_MAX])
{
    nostos_message_t message = {
        .type = NOSTOS_STOP,
        .source_id = source,
        .session_id = session,
        .sequence = sequence,
    };
    size_t length = 0U;
    CHECK(nostos_message_encode(
        &message, wire, NOSTOS_WIRE_MAX, &length) == NOSTOS_OK);
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
    uint8_t wire[NOSTOS_WIRE_MAX];

    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);
    size_t length = encode_stop(1U, 40U, 1U, wire);
    CHECK(nostos_bridge_accept(
        &bridge, NOSTOS_TO_MESH, wire, length, 0U, 10U, true) == NOSTOS_OK);
    length = encode_stop(2U, 90U, 1U, wire);
    CHECK(nostos_bridge_accept(
        &bridge, NOSTOS_TO_UART, wire, length, 0x00D6U, 11U, true) == NOSTOS_OK);
    CHECK(bridge.count == 2U);

    /* Runtime uses this locked re-init at a true STM HELLO boundary. */
    CHECK(nostos_bridge_init(&bridge, 1U, peers) == NOSTOS_OK);
    CHECK(bridge.count == 0U);
    CHECK(bridge.urgent_count == 0U && bridge.normal_count == 0U);
    CHECK(bridge.free_count == NOSTOS_BRIDGE_CAPACITY);
    CHECK(bridge.local_source == 1U);
    CHECK(bridge.peers[1].mesh_address == 0x00D6U);

    nostos_job_t job;
    CHECK(nostos_bridge_next(&bridge, 12U, true, &job) == NOSTOS_EMPTY);
    length = encode_stop(1U, 41U, 1U, wire);
    CHECK(nostos_bridge_accept(
        &bridge, NOSTOS_TO_MESH, wire, length, 0U, 13U, true) == NOSTOS_OK);
    CHECK(nostos_bridge_next(&bridge, 14U, true, &job) == NOSTOS_OK);
    CHECK(job.wire[2] == 1U && job.wire[3] == 41U);

    puts("PASS true STM boot boundary discards pre-boot v2 bridge jobs");
    puts("PASS peer/source binding remains in RAM; next session accepts new work");
    return 0;
}
