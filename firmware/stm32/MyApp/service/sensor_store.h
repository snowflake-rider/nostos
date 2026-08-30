#ifndef SENSOR_STORE_H
#define SENSOR_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_STORE_RIDE_STALE_MS 3000U
#define SENSOR_STORE_ENVIRONMENT_STALE_MS 4000U
#define SENSOR_STORE_IMU_STALE_MS 250U

typedef enum {
    SENSOR_QUALITY_UNMEASURED = 0,
    SENSOR_QUALITY_VALID,
    SENSOR_QUALITY_STALE,
    SENSOR_QUALITY_ERROR
} sensor_quality_t;

typedef struct {
    sensor_quality_t quality;
    uint16_t kmh_x10;
    uint32_t distance_mm;
    uint32_t updated_ms;
    uint32_t revision;
} sensor_ride_sample_t;

typedef struct {
    sensor_quality_t quality;
    int16_t temperature_c_x10;
    uint16_t humidity_pct_x10;
    uint32_t updated_ms;
    uint32_t revision;
} sensor_environment_sample_t;

typedef struct {
    sensor_quality_t quality;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    uint32_t updated_ms;
    uint32_t revision;
} sensor_imu_sample_t;

typedef struct {
    sensor_ride_sample_t ride;
    sensor_environment_sample_t environment;
    sensor_imu_sample_t imu;
} sensor_snapshot_t;

/* The STM32 service task is the sole writer. Readers receive a copied snapshot. */
void sensor_store_init(void);
bool sensor_store_update_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm,
    uint32_t now_ms);
bool sensor_store_update_environment(
    bool valid,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint32_t now_ms);
bool sensor_store_update_imu(
    bool valid,
    float accel_x,
    float accel_y,
    float accel_z,
    float gyro_x,
    float gyro_y,
    float gyro_z,
    uint32_t now_ms);
bool sensor_store_snapshot(uint32_t now_ms, sensor_snapshot_t *snapshot);

#endif /* SENSOR_STORE_H */
