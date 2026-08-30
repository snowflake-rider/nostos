#include "sensor_store.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    sensor_snapshot_t snapshot;
    CHECK(!sensor_store_snapshot(0U, &snapshot));
    sensor_store_init();
    CHECK(sensor_store_snapshot(0U, &snapshot));
    CHECK(snapshot.ride.quality == SENSOR_QUALITY_UNMEASURED);

    CHECK(sensor_store_update_ride(true, 228U, 123456U, 10U));
    CHECK(sensor_store_update_environment(true, 253, 610U, 20U));
    CHECK(sensor_store_update_imu(true, 0.1f, 0.2f, 0.9f,
        1.0f, 2.0f, 3.0f, 40U));
    CHECK(sensor_store_snapshot(100U, &snapshot));
    CHECK(snapshot.ride.quality == SENSOR_QUALITY_VALID);
    CHECK(snapshot.ride.kmh_x10 == 228U);
    CHECK(snapshot.ride.distance_mm == 123456U);
    CHECK(snapshot.environment.temperature_c_x10 == 253);
    CHECK(snapshot.environment.humidity_pct_x10 == 610U);
    CHECK(snapshot.imu.gyro_z == 3.0f);

    CHECK(sensor_store_snapshot(3040U, &snapshot));
    CHECK(snapshot.ride.quality == SENSOR_QUALITY_STALE);
    CHECK(snapshot.imu.quality == SENSOR_QUALITY_STALE);
    CHECK(snapshot.environment.quality == SENSOR_QUALITY_VALID);

    CHECK(sensor_store_update_ride(false, 0U, 0U, 4000U));
    CHECK(!sensor_store_update_ride(false, 1U, 0U, 4000U));
    CHECK(!sensor_store_update_ride(false, 0U, 1U, 4000U));
    CHECK(sensor_store_snapshot(4000U, &snapshot));
    CHECK(snapshot.ride.quality == SENSOR_QUALITY_UNMEASURED);
    CHECK(snapshot.ride.kmh_x10 == 0U);
    CHECK(snapshot.ride.distance_mm == 0U);
    return 0;
}
