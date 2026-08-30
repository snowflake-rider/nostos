#ifndef SENSOR_VIEW_SERVICE_H
#define SENSOR_VIEW_SERVICE_H

#include "sensor_store.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    sensor_snapshot_t sensors;
    uint8_t ride_source_id;
    uint8_t environment_source_id;
} sensor_view_snapshot_t;

/* The display mirror is written only by accepted ESP32 OUTPUT_* commands. */
void sensor_view_service_init(void);
void sensor_view_service_clear_outputs(void);
bool sensor_view_service_apply_output_ride(
    uint8_t source_id,
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms);
bool sensor_view_service_apply_output_environment(
    uint8_t source_id,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint32_t now_ms);
bool sensor_view_service_snapshot(
    uint32_t now_ms,
    sensor_view_snapshot_t *snapshot);

#endif /* SENSOR_VIEW_SERVICE_H */
