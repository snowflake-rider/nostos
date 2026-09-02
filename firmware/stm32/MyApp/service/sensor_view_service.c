#include "sensor_view_service.h"

#include <stddef.h>

static sensor_view_snapshot_t output_view;
static uint32_t next_revision;
static bool initialized;

static uint32_t revision_next(void)
{
    ++next_revision;
    if (next_revision == 0U) {
        next_revision = 1U;
    }
    return next_revision;
}

static bool valid_source(uint8_t source_id)
{
    return source_id >= 1U && source_id <= 10U;
}

static sensor_feed_freshness_t freshness_at(
    uint32_t updated_ms,
    uint32_t now_ms)
{
    uint32_t age_ms = (uint32_t)(now_ms - updated_ms);
    if (age_ms > SENSOR_VIEW_UNKNOWN_AFTER_MS) return SENSOR_FEED_UNKNOWN;
    if (age_ms > SENSOR_VIEW_STALE_AFTER_MS) return SENSOR_FEED_STALE;
    return SENSOR_FEED_FRESH;
}

void sensor_view_service_init(void)
{
    output_view = (sensor_view_snapshot_t){0};
    next_revision = 0U;
    initialized = true;
}

void sensor_view_service_clear_outputs(void)
{
    if (initialized) {
        output_view = (sensor_view_snapshot_t){0};
    }
}

bool sensor_view_service_apply_output_ride(
    uint8_t source_id,
    bool sensor_valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms)
{
    if (!initialized || !valid_source(source_id) ||
        (!sensor_valid && (kmh_x10 != 0U || distance_mm != 0U))) {
        return false;
    }
    output_view.sensors.ride = (sensor_ride_sample_t){
        .quality = sensor_valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_ERROR,
        .kmh_x10 = sensor_valid ? kmh_x10 : 0U,
        .distance_mm = sensor_valid ? distance_mm : 0U,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    output_view.ride_source_id = source_id;
    output_view.ride_sensor_valid = sensor_valid;
    output_view.ride_freshness = SENSOR_FEED_FRESH;
    return true;
}

bool sensor_view_service_apply_output_environment(
    uint8_t source_id,
    bool sensor_valid,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint32_t now_ms)
{
    if (!initialized || !valid_source(source_id) ||
        (!sensor_valid &&
         (temperature_c_x10 != 0 || humidity_pct_x10 != 0U))) {
        return false;
    }
    output_view.sensors.environment = (sensor_environment_sample_t){
        .quality = sensor_valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_ERROR,
        .temperature_c_x10 = sensor_valid ? temperature_c_x10 : 0,
        .humidity_pct_x10 = sensor_valid ? humidity_pct_x10 : 0U,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    output_view.environment_source_id = source_id;
    output_view.environment_sensor_valid = sensor_valid;
    output_view.environment_freshness = SENSOR_FEED_FRESH;
    return true;
}

bool sensor_view_service_snapshot(
    uint32_t now_ms,
    sensor_view_snapshot_t *snapshot)
{
    if (!initialized || snapshot == NULL) {
        return false;
    }
    *snapshot = output_view;
    if (snapshot->sensors.ride.revision != 0U) {
        snapshot->ride_freshness = freshness_at(
            snapshot->sensors.ride.updated_ms, now_ms);
    }
    if (snapshot->ride_freshness == SENSOR_FEED_STALE) {
        snapshot->sensors.ride.quality = SENSOR_QUALITY_STALE;
    } else if (snapshot->ride_freshness == SENSOR_FEED_UNKNOWN) {
        snapshot->sensors.ride = (sensor_ride_sample_t){0};
        snapshot->ride_source_id = 0U;
        snapshot->ride_sensor_valid = false;
    }
    if (snapshot->sensors.environment.revision != 0U) {
        snapshot->environment_freshness = freshness_at(
            snapshot->sensors.environment.updated_ms, now_ms);
    }
    if (snapshot->environment_freshness == SENSOR_FEED_STALE) {
        snapshot->sensors.environment.quality = SENSOR_QUALITY_STALE;
    } else if (snapshot->environment_freshness == SENSOR_FEED_UNKNOWN) {
        snapshot->sensors.environment = (sensor_environment_sample_t){0};
        snapshot->environment_source_id = 0U;
        snapshot->environment_sensor_valid = false;
    }
    return true;
}
