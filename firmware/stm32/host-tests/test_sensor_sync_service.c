#include "message_protocol_service.h"
#include "sensor_store.h"
#include "sensor_sync_service.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

static uint32_t tick;
static uint32_t ride_calls;
static bool last_ride_valid;
static uint16_t last_speed;
static uint32_t last_distance;
static unsigned ride_failures;
static uint32_t environment_calls;
static nostos_quality_t last_environment_quality;

uint32_t HAL_GetTick(void)
{
    return tick;
}

nostos_result_t message_protocol_service_publish_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm)
{
    ++ride_calls;
    last_ride_valid = valid;
    last_speed = kmh_x10;
    last_distance = distance_mm;
    if (ride_failures != 0U) {
        --ride_failures;
        return NOSTOS_IO_ERROR;
    }
    return NOSTOS_OK;
}

nostos_result_t message_protocol_service_publish_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    nostos_quality_t quality)
{
    CHECK(temperature_c_x10 == 253);
    CHECK(humidity_pct_x10 == 610U);
    ++environment_calls;
    last_environment_quality = quality;
    return NOSTOS_OK;
}

int main(void)
{
    sensor_store_init();
    sensor_sync_service_init();
    CHECK(sensor_store_update_ride(true, 228U, 123456U, 10U));
    CHECK(sensor_store_update_environment(true, 253, 610U, 10U));
    CHECK(sensor_store_update_imu(true, 0.1f, -0.2f, 1.0f,
        1.2f, -2.3f, 3.4f, 10U));

    tick = 10U;
    sensor_sync_service_process();
    CHECK(ride_calls == 1U);
    CHECK(last_ride_valid && last_speed == 228U);
    CHECK(last_distance == 123456U);
    CHECK(environment_calls == 1U);
    CHECK(last_environment_quality == NOSTOS_VALID);

    /* Internal 50ms MPU samples never create a Mesh publication. */
    for (tick = 60U; tick <= 960U; tick += 50U) {
        CHECK(sensor_store_update_imu(true, 0.2f, -0.1f, 1.0f,
            2.0f, -1.0f, 0.0f, tick));
        sensor_sync_service_process();
        CHECK(ride_calls == 1U);
        CHECK(environment_calls == 1U);
    }

    tick = 1010U;
    sensor_sync_service_process();
    CHECK(ride_calls == 2U);
    CHECK(environment_calls == 2U);

    tick = 3010U;
    sensor_sync_service_process();
    CHECK(ride_calls == 3U);
    CHECK(!last_ride_valid && last_speed == 0U && last_distance == 0U);
    CHECK(environment_calls == 3U);

    tick = 4010U;
    sensor_sync_service_process();
    CHECK(environment_calls == 4U);
    CHECK(last_environment_quality == NOSTOS_UNMEASURED);

    CHECK(sensor_store_update_ride(true, 315U, 654321U, 5000U));
    CHECK(sensor_store_update_environment(true, 253, 610U, 5000U));
    ride_failures = 1U;
    tick = 5000U;
    sensor_sync_service_process();
    CHECK(ride_calls == 4U);
    CHECK(environment_calls == 4U);
    tick = 5199U;
    sensor_sync_service_process();
    CHECK(ride_calls == 4U);
    tick = 5200U;
    sensor_sync_service_process();
    CHECK(ride_calls == 5U);
    CHECK(last_ride_valid && last_speed == 315U && last_distance == 654321U);
    CHECK(environment_calls == 5U);
    return 0;
}
