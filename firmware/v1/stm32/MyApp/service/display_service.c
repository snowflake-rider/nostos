/* Minimal local DHT11 display adapted from stm32-project 940ff240. */
#include "display_service.h"

#include "app_config.h"
#include "environment_service.h"
#include "ssd1306.h"

#include <stdint.h>

#define DISPLAY_PERIOD_MS 200U
#define DISPLAY_RETRY_MS 2000U

static I2C_HandleTypeDef *display_i2c;
static bool display_ready;
static uint32_t display_tick;

#if FEATURE_SSD1306_DISPLAY
static char *append_text(char *out, const char *text)
{
    while (*text != '\0') *out++ = *text++;
    return out;
}

static char *append_u16_x10(char *out, uint16_t value)
{
    uint16_t whole = value / 10U;
    char reversed[5];
    uint8_t count = 0U;
    do
    {
        reversed[count++] = (char)('0' + (whole % 10U));
        whole /= 10U;
    } while ((whole != 0U) && (count < sizeof(reversed)));
    while (count != 0U) *out++ = reversed[--count];
    *out++ = '.';
    *out++ = (char)('0' + (value % 10U));
    return out;
}

static char *append_i16_x10(char *out, int16_t value)
{
    int32_t wide = value;
    if (wide < 0)
    {
        *out++ = '-';
        wide = -wide;
    }
    return append_u16_x10(out, (uint16_t)wide);
}

static char *append_u32(char *out, uint32_t value)
{
    char reversed[10];
    uint8_t count = 0U;
    do
    {
        reversed[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(reversed)));
    while (count != 0U) *out++ = reversed[--count];
    return out;
}

static void render_environment(void)
{
    char line[22];
    int16_t temperature_x10 = 0;
    uint16_t humidity_x10 = 0U;
    bool valid = environment_service_get(&temperature_x10, &humidity_x10);

    ssd1306_clear();
    ssd1306_draw_text(0U, 0U, "NOSTOS SENSOR");

    char *out = append_text(line, "TEMP ");
    out = valid ? append_i16_x10(out, temperature_x10) : append_text(out, "--.-");
    out = append_text(out, " C");
    *out = '\0';
    ssd1306_draw_text(0U, 16U, line);

    out = append_text(line, "HUM  ");
    out = valid ? append_u16_x10(out, humidity_x10) : append_text(out, "--.-");
    out = append_text(out, " %");
    *out = '\0';
    ssd1306_draw_text(0U, 32U, line);

    if (valid)
    {
        ssd1306_draw_text(0U, 48U, "DHT OK");
    }
    else
    {
        uint32_t failures = environment_service_failure_count();
        out = append_text(line, failures == 0U ? "DHT WAIT" : "DHT ERROR ");
        if (failures != 0U) out = append_u32(out, failures);
        *out = '\0';
        ssd1306_draw_text(0U, 48U, line);
    }
}

static bool initialize_and_render(void)
{
    if (!ssd1306_init(display_i2c)) return false;
    render_environment();
    return ssd1306_update();
}
#endif

void display_service_init(I2C_HandleTypeDef *i2c)
{
    display_i2c = i2c;
#if FEATURE_SSD1306_DISPLAY
    display_ready = (i2c != NULL) && initialize_and_render();
#else
    display_ready = false;
#endif
    display_tick = HAL_GetTick();
}

void display_service_process(void)
{
#if FEATURE_SSD1306_DISPLAY
    uint32_t now = HAL_GetTick();
    uint32_t period = display_ready ? DISPLAY_PERIOD_MS : DISPLAY_RETRY_MS;
    if ((uint32_t)(now - display_tick) < period) return;
    display_tick = now;

    if (!display_ready)
    {
        display_ready = initialize_and_render();
        return;
    }

    render_environment();
    display_ready = ssd1306_update();
#endif
}

bool display_service_is_ready(void)
{
    return display_ready;
}
