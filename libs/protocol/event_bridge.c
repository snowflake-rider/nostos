#include "event_bridge.h"

void event_bridge_init(event_bridge_t *bridge)
{
    if (bridge != NULL) *bridge = (event_bridge_t){0};
}

event_result_t event_bridge_uart(event_bridge_t *bridge, uint8_t id,
                                 uint64_t now_ms, bool mesh_ready)
{
    if (bridge == NULL) return EVENT_INVALID;
    if (id == MSG_NONE) {
        bridge->stats.uart_noop++;
        return EVENT_NOOP;
    }
    if (!event_id_valid(id)) {
        bridge->stats.uart_invalid++;
        return EVENT_INVALID;
    }
    bridge->stats.uart_valid++;
    if (!mesh_ready) {
        bridge->stats.not_ready++;
        return EVENT_NOT_READY;
    }
    if (bridge->count == EVENT_QUEUE_CAPACITY) {
        bridge->stats.full[EVENT_TO_MESH]++;
        return EVENT_FULL;
    }
    bridge->jobs[(bridge->head + bridge->count) % EVENT_QUEUE_CAPACITY] =
        (event_job_t){.received_ms = now_ms, .id = id, .direction = EVENT_TO_MESH};
    bridge->count++;
    return EVENT_OK;
}

bool event_job_send(const event_job_t *job, const event_transport_t *transport)
{
    if (job == NULL || transport == NULL || !event_id_valid(job->id)) return false;
    if (job->direction == EVENT_TO_UART) {
        return transport->uart != NULL && transport->uart(transport->context, &job->id, 1);
    }
    if (job->direction == EVENT_TO_MESH) {
        uint8_t wire[EVENT_WIRE_SIZE];
        return event_encode(job->id, wire, sizeof(wire)) && transport->mesh != NULL &&
               transport->mesh(transport->context, wire, sizeof(wire));
    }
    return false;
}

void event_bridge_complete(event_bridge_t *bridge, event_direction_t direction, bool accepted)
{
    if (bridge == NULL || (direction != EVENT_TO_MESH && direction != EVENT_TO_UART)) return;
    if (accepted) bridge->stats.accepted[direction]++;
    else bridge->stats.failed[direction]++;
}

event_stats_t event_bridge_stats(const event_bridge_t *bridge)
{
    return bridge != NULL ? bridge->stats : (event_stats_t){0};
}

event_result_t event_bridge_mesh(event_bridge_t *bridge, const uint8_t *wire,
                                 size_t length, uint16_t source,
                                 uint16_t own_address, uint64_t now_ms)
{
    if (bridge == NULL) return EVENT_INVALID;
    uint8_t id;
    if (!event_decode(wire, length, &id) || source == 0 || source >= 0x8000) {
        bridge->stats.mesh_invalid++;
        return EVENT_INVALID;
    }
    bridge->stats.mesh_valid++;
    if (source == own_address) {
        bridge->stats.mesh_self++;
        return EVENT_SELF;
    }
    if (bridge->count == EVENT_QUEUE_CAPACITY) {
        bridge->stats.full[EVENT_TO_UART]++;
        return EVENT_FULL;
    }
    bridge->jobs[(bridge->head + bridge->count) % EVENT_QUEUE_CAPACITY] =
        (event_job_t){.received_ms = now_ms, .source = source,
                      .id = id, .direction = EVENT_TO_UART};
    bridge->count++;
    return EVENT_OK;
}

event_result_t event_bridge_next(event_bridge_t *bridge, uint64_t now_ms,
                                 bool mesh_ready, event_job_t *job)
{
    if (bridge == NULL || job == NULL) return EVENT_INVALID;
    if (bridge->count == 0) return EVENT_EMPTY;
    *job = bridge->jobs[bridge->head];
    bridge->head = (bridge->head + 1) % EVENT_QUEUE_CAPACITY;
    bridge->count--;
    if (now_ms < job->received_ms || now_ms - job->received_ms >= EVENT_MAX_AGE_MS) {
        bridge->stats.expired[job->direction]++;
        return EVENT_EXPIRED;
    }
    if (job->direction == EVENT_TO_MESH && !mesh_ready) {
        bridge->stats.not_ready++;
        return EVENT_NOT_READY;
    }
    return EVENT_OK;
}
