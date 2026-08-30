#include "sensor_store.h"

#include <math.h>
#include <stddef.h>

static sensor_snapshot_t current;
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

static sensor_quality_t quality_at(
    sensor_quality_t quality,
    uint32_t updated_ms,
    uint32_t now_ms,
    uint32_t stale_after_ms)
{
    if (quality == SENSOR_QUALITY_VALID &&
        (uint32_t)(now_ms - updated_ms) >= stale_after_ms) {
        return SENSOR_QUALITY_STALE;
    }
    return quality;
}

void sensor_store_init(void)
{
    current = (sensor_snapshot_t){0};
    next_revision = 0U;
    initialized = true;
}

bool sensor_store_update_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms)
{
    if (!initialized || (!valid && (kmh_x10 != 0U || distance_mm != 0U))) {
        return false;
    }
    current.ride = (sensor_ride_sample_t){
        .quality = valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_UNMEASURED,
        .kmh_x10 = valid ? kmh_x10 : 0U,
        .distance_mm = valid ? distance_mm : 0U,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    return true;
}

bool sensor_store_update_environment(
    bool valid,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint32_t now_ms)
{
    if (!initialized) {
        return false;
    }
    current.environment = (sensor_environment_sample_t){
        .quality = valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_ERROR,
        .temperature_c_x10 = valid ? temperature_c_x10 : 0,
        .humidity_pct_x10 = valid ? humidity_pct_x10 : 0U,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    return true;
}

bool sensor_store_update_imu(
    bool valid,
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z,
    uint32_t now_ms)
{
    if (!initialized || (valid &&
        (!isfinite(accel_x) || !isfinite(accel_y) || !isfinite(accel_z) ||
         !isfinite(gyro_x) || !isfinite(gyro_y) || !isfinite(gyro_z)))) {
        return false;
    }
    current.imu = (sensor_imu_sample_t){
        .quality = valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_ERROR,
        .accel_x = valid ? accel_x : 0.0f,
        .accel_y = valid ? accel_y : 0.0f,
        .accel_z = valid ? accel_z : 0.0f,
        .gyro_x = valid ? gyro_x : 0.0f,
        .gyro_y = valid ? gyro_y : 0.0f,
        .gyro_z = valid ? gyro_z : 0.0f,
        .updated_ms = now_ms,
        .revision = revision_next(),
    };
    return true;
}

bool sensor_store_snapshot(uint32_t now_ms, sensor_snapshot_t *snapshot)
{
    if (!initialized || snapshot == NULL) {
        return false;
    }
    *snapshot = current;
    snapshot->ride.quality = quality_at(snapshot->ride.quality,
        snapshot->ride.updated_ms, now_ms, SENSOR_STORE_RIDE_STALE_MS);
    snapshot->environment.quality = quality_at(snapshot->environment.quality,
        snapshot->environment.updated_ms, now_ms, SENSOR_STORE_ENVIRONMENT_STALE_MS);
    snapshot->imu.quality = quality_at(snapshot->imu.quality,
        snapshot->imu.updated_ms, now_ms, SENSOR_STORE_IMU_STALE_MS);
    return true;
}
