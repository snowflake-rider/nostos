#ifndef BSG_EVENT_BRIDGE_H
#define BSG_EVENT_BRIDGE_H
#include "event_protocol.h"

#define EVENT_QUEUE_CAPACITY 32U
#define EVENT_MAX_AGE_MS 1000U
typedef enum { EVENT_TO_MESH, EVENT_TO_UART, EVENT_DIRECTION_COUNT } event_direction_t;
typedef enum {
    EVENT_OK, EVENT_EMPTY, EVENT_NOOP, EVENT_INVALID, EVENT_SELF,
    EVENT_NOT_READY, EVENT_FULL, EVENT_EXPIRED
} event_result_t;
typedef struct {
    uint64_t received_ms;
    uint16_t source;
    uint8_t id;
    event_direction_t direction;
} event_job_t;
typedef struct {
    uint32_t uart_valid, uart_noop, uart_invalid;
    uint32_t mesh_valid, mesh_invalid, mesh_self;
    uint32_t not_ready;
    uint32_t full[EVENT_DIRECTION_COUNT], expired[EVENT_DIRECTION_COUNT];
    uint32_t accepted[EVENT_DIRECTION_COUNT], failed[EVENT_DIRECTION_COUNT];
} event_stats_t;
/* Fixed storage. Caller serializes ALL access, including stats, in RTOS builds. */
typedef struct {
    event_job_t jobs[EVENT_QUEUE_CAPACITY];
    size_t head, count;
    event_stats_t stats;
} event_bridge_t;

void event_bridge_init(event_bridge_t *bridge);
event_result_t event_bridge_uart(event_bridge_t *bridge, uint8_t id,
                                 uint64_t now_ms, bool mesh_ready);
event_result_t event_bridge_mesh(event_bridge_t *bridge, const uint8_t *wire,
                                 size_t length, uint16_t source,
                                 uint16_t own_address, uint64_t now_ms);
event_result_t event_bridge_next(event_bridge_t *bridge, uint64_t now_ms,
                                 bool mesh_ready, event_job_t *job);
typedef bool (*event_send_fn)(void *context, const uint8_t *bytes, size_t length);
typedef struct {
    void *context;
    event_send_fn mesh, uart;
} event_transport_t;
/* Call outside queue lock. Transport must consume/copy bytes before returning. */
bool event_job_send(const event_job_t *job, const event_transport_t *transport);
void event_bridge_complete(event_bridge_t *bridge, event_direction_t direction, bool accepted);
event_stats_t event_bridge_stats(const event_bridge_t *bridge);
#endif
