#ifndef NOSTOS_CASE_HELPERS_H
#define NOSTOS_CASE_HELPERS_H

#include "nostos_bridge.h"
#include "nostos_state.h"

#include <stdio.h>
#include <stdlib.h>

#define CASE_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static const nostos_peer_t case_peers[NOSTOS_NODE_COUNT] = {
    {0x0101U, 1U, 1U}, {0x0202U, 2U, 2U}, {0x0303U, 3U, 3U}
};

static inline nostos_message_t case_message(uint8_t type, uint8_t source,
                                            uint16_t sequence, uint16_t incident)
{
    nostos_message_t message = {
        .type = type,
        .source_id = source,
        .session_id = 1U,
        .sequence = sequence
    };
    message.payload.incident = (nostos_incident_ref_t){1U, incident};
    return message;
}

static inline size_t case_encode(nostos_message_t message,
                                 uint8_t wire[NOSTOS_WIRE_MAX])
{
    size_t length = 0U;
    CASE_CHECK(nostos_message_encode(&message, wire, NOSTOS_WIRE_MAX, &length) == NOSTOS_OK);
    return length;
}

static inline nostos_receiver_t case_receiver(uint8_t local_source)
{
    nostos_receiver_t receiver;
    CASE_CHECK(nostos_receiver_init(&receiver, local_source) == NOSTOS_OK);
    for (uint8_t source = 1U; source <= NOSTOS_NODE_COUNT; ++source) {
        CASE_CHECK(nostos_receiver_approve_session(&receiver, source, 1U, 0U) == NOSTOS_OK);
    }
    return receiver;
}

#endif
