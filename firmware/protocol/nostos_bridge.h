#ifndef NOSTOS_BRIDGE_H
#define NOSTOS_BRIDGE_H
#include "nostos_uart.h"
#define NOSTOS_BRIDGE_CAPACITY 16U
#define NOSTOS_BRIDGE_URGENT_RESERVE 4U
#define NOSTOS_BRIDGE_NONURGENT_CAPACITY \
    (NOSTOS_BRIDGE_CAPACITY-NOSTOS_BRIDGE_URGENT_RESERVE)
#define NOSTOS_BRIDGE_STOP_RESERVE 1U
#define NOSTOS_BRIDGE_NORMAL_CAPACITY \
    (NOSTOS_BRIDGE_NONURGENT_CAPACITY-NOSTOS_BRIDGE_STOP_RESERVE)
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
    uint8_t urgent_order[NOSTOS_BRIDGE_CAPACITY];
    uint8_t stop_order[NOSTOS_BRIDGE_NONURGENT_CAPACITY];
    uint8_t normal_order[NOSTOS_BRIDGE_NORMAL_CAPACITY];
    uint8_t free_slots[NOSTOS_BRIDGE_CAPACITY];
    size_t urgent_head, urgent_count;
    size_t stop_head, stop_count;
    size_t normal_head, normal_count;
    size_t free_count, count;
} nostos_bridge_t;
/* Caller serializes calls; no locks, hardware, dynamic allocation or TTL here.
 * FALL and FALL_CLEAR messages keep four slots unavailable to normal
 * traffic and one additional slot remains unavailable to normal traffic so a
 * STOP can always enter ahead of it. Dispatch is strict FALL/CLEAR, then STOP,
 * then normal FIFO. */
nostos_result_t nostos_bridge_init(nostos_bridge_t *bridge, uint8_t local_source,
    const nostos_peer_t peers[NOSTOS_NODE_COUNT]);
nostos_result_t nostos_bridge_accept(nostos_bridge_t *bridge, nostos_direction_t direction,
    const uint8_t *wire, size_t length, uint16_t mesh_source, uint32_t now_ms, bool mesh_ready);
nostos_result_t nostos_bridge_next(nostos_bridge_t *bridge, uint32_t now_ms, bool mesh_ready, nostos_job_t *job);
#endif
