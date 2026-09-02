#include "message_protocol_service.h"
#include "sensor_store.h"
#include "sensor_sync_service.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

static uint32_t tick;
static bool link_ready;
static uint32_t ride_calls;
static bool last_ride_valid;
static uint16_t last_speed;
static uint32_t last_distance;
static unsigned ride_failures;
static uint32_t environment_calls;
static int16_t last_temperature;
static uint16_t last_humidity;
static bool last_environment_valid;

uint32_t HAL_GetTick(void)
{
    return tick;
}

bool message_protocol_service_is_ready(void)
{
    return link_ready;
}

message_protocol_result_t message_protocol_service_publish_ride(
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
        return MESSAGE_PROTOCOL_IO_ERROR;
    }
    return MESSAGE_PROTOCOL_OK;
}

message_protocol_result_t message_protocol_service_publish_environment(
    bool valid,
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10)
{
    ++environment_calls;
    last_environment_valid = valid;
    last_temperature = temperature_c_x10;
    last_humidity = humidity_pct_x10;
    return MESSAGE_PROTOCOL_OK;
}

static void reset_fixture(void)
{
    tick = 0U;
    link_ready = false;
    ride_calls = 0U;
    last_ride_valid = false;
    last_speed = 0U;
    last_distance = 0U;
    ride_failures = 0U;
    environment_calls = 0U;
    last_temperature = 0;
    last_humidity = 0U;
    last_environment_valid = false;
    sensor_store_init();
    sensor_sync_service_init();
}

int main(void)
{
    reset_fixture();
    CHECK(sensor_store_update_ride(true, 228U, 123456U, 10U));
    CHECK(sensor_store_update_environment(true, 253, 610U, 10U));

    tick = 10U;
    sensor_sync_service_process();
    CHECK(ride_calls == 0U && environment_calls == 0U);

    link_ready = true;
    sensor_sync_service_process();
    CHECK(ride_calls == 1U);
    CHECK(last_ride_valid && last_speed == 228U);
    CHECK(last_distance == 123U);
    CHECK(environment_calls == 1U);
    CHECK(last_temperature == 253 && last_humidity == 610U);
    CHECK(last_environment_valid);

    tick = 1010U;
    sensor_sync_service_process();
    CHECK(ride_calls == 1U && environment_calls == 1U);

    tick = 2010U;
    sensor_sync_service_process();
    CHECK(ride_calls == 2U);
    CHECK(last_ride_valid && last_speed == 228U && last_distance == 123U);
    CHECK(environment_calls == 2U && last_environment_valid);

    tick = 4010U;
    sensor_sync_service_process();
    CHECK(ride_calls == 3U && environment_calls == 3U);
    CHECK(!last_ride_valid && last_speed == 0U && last_distance == 0U);
    CHECK(!last_environment_valid);
    CHECK(last_temperature == 0 && last_humidity == 0U);

    ride_failures = 1U;
    CHECK(sensor_store_update_ride(true, 315U, 654321U, 5000U));
    CHECK(sensor_store_update_environment(true, 271, 580U, 5000U));
    tick = 5000U;
    sensor_sync_service_process();
    CHECK(ride_calls == 4U);
    CHECK(environment_calls == 3U);
    tick = 5199U;
    sensor_sync_service_process();
    CHECK(ride_calls == 4U);
    tick = 5200U;
    sensor_sync_service_process();
    CHECK(ride_calls == 5U && environment_calls == 4U);
    CHECK(last_ride_valid && last_speed == 315U && last_distance == 654U);
    CHECK(last_temperature == 271 && last_humidity == 580U);

    puts("PASS sensor producers publish changes, repeat at 2s, and retry I/O");
    return 0;
}
