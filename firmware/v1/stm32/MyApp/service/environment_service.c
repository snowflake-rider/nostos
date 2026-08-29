/* Local-only adapter for the DHT11 producer from stm32-project 940ff240. */
#include "environment_service.h"

#include "app_config.h"
#include "dht11.h"
#include "stm32f4xx_hal.h"

#include <stddef.h>

#define DHT11_SAMPLE_PERIOD_MS 1200U

static uint32_t failure_count;
static bool data_valid;
static dht11_data_t latest_data;
#if FEATURE_DHT11_SENSOR
static uint32_t sample_tick;
static bool sensor_initialized;
#endif

void environment_service_init(void)
{
    failure_count = 0U;
    data_valid = false;
    latest_data = (dht11_data_t){0};
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
            data_valid = false;
            ++failure_count;
            return;
        }
    }

    dht11_data_t sample;
    if (!dht11_read(&sample))
    {
        data_valid = false;
        ++failure_count;
        return;
    }

    latest_data = sample;
    data_valid = true;
#endif
}

bool environment_service_data_valid(void)
{
    return data_valid;
}

bool environment_service_get(int16_t *temperature_x10, uint16_t *humidity_x10)
{
    if (!data_valid || (temperature_x10 == NULL) || (humidity_x10 == NULL))
    {
        return false;
    }
    *temperature_x10 = latest_data.temperature_x10;
    *humidity_x10 = latest_data.humidity_x10;
    return true;
}

uint32_t environment_service_failure_count(void)
{
    return failure_count;
}
