#ifndef SENSOR_VIEW_SERVICE_H
#define SENSOR_VIEW_SERVICE_H

#include "sensor_store.h"

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_VIEW_STALE_AFTER_MS 6000U
#define SENSOR_VIEW_UNKNOWN_AFTER_MS 20000U

typedef enum {
    SENSOR_FEED_UNKNOWN = 0,
    SENSOR_FEED_FRESH,
    SENSOR_FEED_STALE,
} sensor_feed_freshness_t;

typedef struct {
    sensor_snapshot_t sensors;
    uint8_t ride_source_id;
    uint8_t environment_source_id;
    bool ride_sensor_valid;
    bool environment_sensor_valid;
    sensor_feed_freshness_t ride_freshness;
    sensor_feed_freshness_t environment_freshness;
} sensor_view_snapshot_t;

/* The display mirror is written only by validated ESP32 STATE_UPDATE messages. */
void sensor_view_service_init(void);
void sensor_view_service_clear_outputs(void);
bool sensor_view_service_apply_output_ride(
    uint8_t source_id,
    bool sensor_valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms);
bool sensor_view_service_apply_output_environment(
    uint8_t source_id,
    bool sensor_valid,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint32_t now_ms);
bool sensor_view_service_snapshot(
    uint32_t now_ms,
    sensor_view_snapshot_t *snapshot);

#endif /* SENSOR_VIEW_SERVICE_H */
