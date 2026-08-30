#include "sensor_view_service.h"

#include "sensor_link.h"

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
    return source_id >= SENSOR_LINK_SOURCE_ID_MIN &&
        source_id <= SENSOR_LINK_SOURCE_ID_MAX;
}

static sensor_quality_t environment_quality(
    uint8_t temperature_quality,
    uint8_t humidity_quality)
{
    if (temperature_quality == SENSOR_LINK_QUALITY_VALID &&
        humidity_quality == SENSOR_LINK_QUALITY_VALID) {
        return SENSOR_QUALITY_VALID;
    }
    if (temperature_quality == SENSOR_LINK_QUALITY_UNMEASURED &&
        humidity_quality == SENSOR_LINK_QUALITY_UNMEASURED) {
        return SENSOR_QUALITY_UNMEASURED;
    }
    return SENSOR_QUALITY_ERROR;
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
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms)
{
    if (!initialized || !valid_source(source_id) ||
        (!valid && (kmh_x10 != 0U || distance_mm != 0U))) {
        return false;
    }
    output_view.sensors.ride = (sensor_ride_sample_t){
        .quality = valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_UNMEASURED,
        .kmh_x10 = valid ? kmh_x10 : 0U,
        .distance_mm = valid ? distance_mm : 0U,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    output_view.ride_source_id = valid ? source_id : 0U;
    return true;
}

bool sensor_view_service_apply_output_environment(
    uint8_t source_id,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t temperature_quality,
    uint8_t humidity_quality,
    uint32_t now_ms)
{
    if (!initialized || !valid_source(source_id) ||
        temperature_quality > SENSOR_LINK_QUALITY_MAX ||
        humidity_quality > SENSOR_LINK_QUALITY_MAX) {
        return false;
    }
    sensor_quality_t quality = environment_quality(
        temperature_quality, humidity_quality);
    output_view.sensors.environment = (sensor_environment_sample_t){
        .quality = quality,
        .temperature_c_x10 = temperature_c_x10,
        .humidity_pct_x10 = humidity_pct_x10,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    output_view.environment_source_id =
        quality == SENSOR_QUALITY_VALID ? source_id : 0U;
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
    if (snapshot->sensors.ride.quality == SENSOR_QUALITY_VALID &&
        (uint32_t)(now_ms - snapshot->sensors.ride.updated_ms) >=
            SENSOR_STORE_RIDE_STALE_MS) {
        snapshot->sensors.ride.quality = SENSOR_QUALITY_STALE;
        snapshot->ride_source_id = 0U;
    }
    if (snapshot->sensors.environment.quality == SENSOR_QUALITY_VALID &&
        (uint32_t)(now_ms - snapshot->sensors.environment.updated_ms) >=
            SENSOR_STORE_ENVIRONMENT_STALE_MS) {
        snapshot->sensors.environment.quality = SENSOR_QUALITY_STALE;
        snapshot->environment_source_id = 0U;
    }
    return true;
}
