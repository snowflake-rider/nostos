#ifndef SENSOR_VIEW_SERVICE_H
#define SENSOR_VIEW_SERVICE_H

#include "nostos_state.h"
#include "sensor_store.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    sensor_snapshot_t sensors;
    uint8_t ride_source_id;
    uint8_t environment_source_id;
} sensor_view_snapshot_t;

/* The network pointer is read only and remains owned by the protocol endpoint.
 * Both services run sequentially in the STM32 service task. */
void sensor_view_service_init(void);
void sensor_view_service_bind_network(
    const nostos_network_state_t *network,
    uint8_t local_source_id);

/* Local valid samples are authoritative. When they are unavailable or stale,
 * choose the freshest valid network report without copying it into sensor_store
 * and therefore without causing a Mesh re-publish loop. */
bool sensor_view_service_snapshot(
    uint32_t now_ms,
    sensor_view_snapshot_t *snapshot);

#endif /* SENSOR_VIEW_SERVICE_H */
