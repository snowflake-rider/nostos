#include "display_service.h"
#include "environment_service.h"
#include "ssd1306.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static uint32_t fake_tick;
static bool fake_oled_init_ok = true;
static bool fake_oled_update_ok = true;
static uint32_t oled_init_count;
static uint32_t oled_update_count;
static char drawn_lines[4][22];
static uint32_t drawn_count;
static bool environment_valid;
static int16_t environment_temperature;
static uint16_t environment_humidity;
static uint32_t environment_failures;

uint32_t HAL_GetTick(void)
{
    return fake_tick;
}

bool ssd1306_init(I2C_HandleTypeDef *i2c)
{
    ++oled_init_count;
    return (i2c != NULL) && fake_oled_init_ok;
}

void ssd1306_clear(void)
{
    drawn_count = 0U;
    memset(drawn_lines, 0, sizeof(drawn_lines));
}

void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text)
{
    (void)x;
    (void)y;
    if (drawn_count < 4U)
    {
        (void)snprintf(
            drawn_lines[drawn_count],
            sizeof(drawn_lines[drawn_count]),
            "%s",
            text
        );
        ++drawn_count;
    }
}

bool ssd1306_update(void)
{
    ++oled_update_count;
    return fake_oled_update_ok;
}

bool environment_service_get(int16_t *temperature_x10, uint16_t *humidity_x10)
{
    if (!environment_valid) return false;
    *temperature_x10 = environment_temperature;
    *humidity_x10 = environment_humidity;
    return true;
}

uint32_t environment_service_failure_count(void)
{
    return environment_failures;
}

int main(void)
{
    I2C_HandleTypeDef i2c = {0};
    display_service_init(&i2c);
    CHECK(display_service_is_ready());
    CHECK(oled_init_count == 1U);
    CHECK(oled_update_count == 1U);
    CHECK(drawn_count == 4U);
    CHECK(strcmp(drawn_lines[0], "NOSTOS SENSOR") == 0);
    CHECK(strcmp(drawn_lines[1], "TEMP --.- C") == 0);
    CHECK(strcmp(drawn_lines[2], "HUM  --.- %") == 0);
    CHECK(strcmp(drawn_lines[3], "DHT WAIT") == 0);

    environment_valid = true;
    environment_temperature = 253;
    environment_humidity = 610U;
    fake_tick = 200U;
    display_service_process();
    CHECK(strcmp(drawn_lines[1], "TEMP 25.3 C") == 0);
    CHECK(strcmp(drawn_lines[2], "HUM  61.0 %") == 0);
    CHECK(strcmp(drawn_lines[3], "DHT OK") == 0);

    environment_valid = false;
    environment_failures = 2U;
    fake_tick = 400U;
    display_service_process();
    CHECK(strcmp(drawn_lines[3], "DHT ERROR 2") == 0);

    fake_oled_update_ok = false;
    fake_tick = 600U;
    display_service_process();
    CHECK(!display_service_is_ready());

    fake_oled_init_ok = true;
    fake_oled_update_ok = true;
    fake_tick = 2599U;
    display_service_process();
    CHECK(oled_init_count == 1U);
    fake_tick = 2600U;
    display_service_process();
    CHECK(oled_init_count == 2U);
    CHECK(display_service_is_ready());

    puts("display_service tests passed");
    return 0;
}
