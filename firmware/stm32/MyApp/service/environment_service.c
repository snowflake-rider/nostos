/* Local-only adapter for the DHT11 producer from stm32-project 940ff240. */
#include "environment_service.h"

#include "app_config.h"
#include "dht11.h"
#include "sensor_store.h"
#include "stm32f4xx_hal.h"

#include <stddef.h>

#define DHT11_SAMPLE_PERIOD_MS 1200U

static uint32_t failure_count;
#if FEATURE_DHT11_SENSOR
static uint32_t sample_tick;
static bool sensor_initialized;
#endif

void environment_service_init(void)
{
    failure_count = 0U;
#if FEATURE_DHT11_SENSOR
    sample_tick = HAL_GetTick();
    sensor_initialized = dht11_init(DHT11_DATA_PORT, DHT11_DATA_PIN);
    if (!sensor_initialized) ++failure_count;
#endif
}

void environment_service_process(void)
{
#if FEATURE_DHT11_SENSOR
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - sample_tick) < DHT11_SAMPLE_PERIOD_MS) return;
    sample_tick = now;

    if (!sensor_initialized)
    {
        sensor_initialized = dht11_init(DHT11_DATA_PORT, DHT11_DATA_PIN);
        if (!sensor_initialized)
        {
            (void)sensor_store_update_environment(false, 0, 0U, now);
            ++failure_count;
            return;
        }
    }

    dht11_data_t sample;
    if (!dht11_read(&sample))
    {
        (void)sensor_store_update_environment(false, 0, 0U, now);
        ++failure_count;
        return;
    }

    (void)sensor_store_update_environment(
        true,
        sample.temperature_x10,
        sample.humidity_x10,
        now
    );
#endif
}

bool environment_service_data_valid(void)
{
    sensor_snapshot_t snapshot;
    return sensor_store_snapshot(HAL_GetTick(), &snapshot) &&
        snapshot.environment.quality == SENSOR_QUALITY_VALID;
}

bool environment_service_get(int16_t *temperature_x10, uint16_t *humidity_x10)
{
    sensor_snapshot_t snapshot;
    if ((temperature_x10 == NULL) || (humidity_x10 == NULL) ||
        !sensor_store_snapshot(HAL_GetTick(), &snapshot) ||
        snapshot.environment.quality != SENSOR_QUALITY_VALID)
    {
        return false;
    }
    *temperature_x10 = snapshot.environment.temperature_c_x10;
    *humidity_x10 = snapshot.environment.humidity_pct_x10;
    return true;
}

uint32_t environment_service_failure_count(void)
{
    return failure_count;
}
