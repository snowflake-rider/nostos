#ifndef NOSTOS_BRIDGE_H
#define NOSTOS_BRIDGE_H
#include "nostos_uart.h"
#define NOSTOS_BRIDGE_CAPACITY 16U
#define NOSTOS_BRIDGE_MAX_AGE_MS 2000U
typedef enum { NOSTOS_TO_MESH=0, NOSTOS_TO_UART=1 } nostos_direction_t;
/* Deployment-owned mapping, not derived from payload assertions. */
typedef struct { uint16_t mesh_address; uint8_t source_id, role; } nostos_peer_t;
typedef struct {
    uint8_t wire[NOSTOS_WIRE_MAX];
    size_t length;
    uint32_t received_ms;
    nostos_direction_t direction;
} nostos_job_t;
typedef struct {
    nostos_peer_t peers[NOSTOS_NODE_COUNT];
    uint8_t local_source;
    nostos_job_t jobs[NOSTOS_BRIDGE_CAPACITY];
    size_t head, count;
} nostos_bridge_t;
/* Caller serializes calls; no locks, hardware, dynamic allocation or TTL here. */
nostos_result_t nostos_bridge_init(nostos_bridge_t *bridge, uint8_t local_source,
    const nostos_peer_t peers[NOSTOS_NODE_COUNT]);
nostos_result_t nostos_bridge_accept(nostos_bridge_t *bridge, nostos_direction_t direction,
    const uint8_t *wire, size_t length, uint16_t mesh_source, uint32_t now_ms, bool mesh_ready);
nostos_result_t nostos_bridge_next(nostos_bridge_t *bridge, uint32_t now_ms, bool mesh_ready, nostos_job_t *job);
#endif
