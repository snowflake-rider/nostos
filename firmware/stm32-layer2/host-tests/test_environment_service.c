#include "dht11.h"
#include "environment_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

GPIO_TypeDef host_gpio_a;
GPIO_TypeDef host_gpio_b;
GPIO_TypeDef host_gpio_c;

static uint32_t fake_tick;
static bool fake_init_ok = true;
static bool fake_read_ok;
static dht11_data_t fake_sample;
static GPIO_TypeDef *initialized_port;
static uint16_t initialized_pin;
static uint32_t init_count;
static uint32_t read_count;

uint32_t HAL_GetTick(void)
{
    return fake_tick;
}

bool dht11_init(GPIO_TypeDef *port, uint16_t pin)
{
    initialized_port = port;
    initialized_pin = pin;
    ++init_count;
    return fake_init_ok;
}

bool dht11_read(dht11_data_t *data)
{
    ++read_count;
    if (!fake_read_ok) return false;
    *data = fake_sample;
    return true;
}

int main(void)
{
    environment_service_init();
    CHECK(init_count == 1U);
    CHECK(initialized_port == GPIOA);
    CHECK(initialized_pin == GPIO_PIN_1);
    CHECK(!environment_service_data_valid());
    CHECK(environment_service_failure_count() == 0U);

    fake_read_ok = true;
    fake_sample = (dht11_data_t){
        .temperature_x10 = 253,
        .humidity_x10 = 610U,
    };
    fake_tick = 1199U;
    environment_service_process();
    CHECK(read_count == 0U);

    fake_tick = 1200U;
    environment_service_process();
    CHECK(read_count == 1U);
    CHECK(environment_service_data_valid());

    int16_t temperature = 0;
    uint16_t humidity = 0U;
    CHECK(environment_service_get(&temperature, &humidity));
    CHECK(temperature == 253);
    CHECK(humidity == 610U);

    fake_read_ok = false;
    fake_tick = 2400U;
    environment_service_process();
    CHECK(read_count == 2U);
    CHECK(!environment_service_data_valid());
    CHECK(environment_service_failure_count() == 1U);
    CHECK(!environment_service_get(&temperature, &humidity));

    puts("environment_service tests passed");
    return 0;
}
