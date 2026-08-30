#include "display_service.h"
#include "message_type.h"
#include "sensor_view_service.h"
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
static char drawn_lines[8][64];
static int16_t drawn_x[8];
static uint8_t drawn_y[8];
static uint32_t drawn_count;
static uint32_t inverted_text_count;
static uint8_t fill_rect_x[4];
static uint8_t fill_rect_y[4];
static uint8_t fill_rect_width[4];
static uint8_t fill_rect_height[4];
static uint32_t fill_rect_count;
static uint32_t outline_rect_count;
static uint8_t outline_rect_x[2];
static uint8_t outline_rect_y[2];
static uint8_t outline_rect_width[2];
static uint8_t outline_rect_height[2];
static uint8_t clear_rect_x[10];
static uint8_t clear_rect_y[10];
static uint32_t clear_rect_count;
static uint8_t inverted_rect_x[10];
static uint8_t inverted_rect_y[10];
static uint32_t inverted_rect_count;
static uint32_t hline_count;
static uint32_t line_count;
static bool frame_inverted;
static uint8_t circle_x[6];
static uint8_t circle_y[6];
static bool circle_filled[6];
static uint32_t circle_count;
static bool environment_valid;
static int16_t environment_temperature;
static uint16_t environment_humidity;
static bool ride_valid;
static uint16_t ride_kmh_x10;
static uint32_t ride_distance_mm;

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
    inverted_text_count = 0U;
    fill_rect_count = 0U;
    outline_rect_count = 0U;
    clear_rect_count = 0U;
    inverted_rect_count = 0U;
    hline_count = 0U;
    line_count = 0U;
    circle_count = 0U;
    frame_inverted = false;
    memset(drawn_lines, 0, sizeof(drawn_lines));
}

static void record_text(int16_t x, uint8_t y, const char *text)
{
    if (drawn_count < 8U)
    {
        drawn_x[drawn_count] = x;
        drawn_y[drawn_count] = y;
        (void)snprintf(
            drawn_lines[drawn_count],
            sizeof(drawn_lines[drawn_count]),
            "%s",
            text
        );
        ++drawn_count;
    }
}

void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text)
{
    record_text(x, y, text);
}

void ssd1306_draw_text_scrolled(int16_t x, uint8_t y, const char *text)
{
    record_text(x, y, text);
}

void ssd1306_draw_text_inverted(uint8_t x, uint8_t y, const char *text)
{
    ++inverted_text_count;
    record_text(x, y, text);
}

void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    if (fill_rect_count < 4U)
    {
        fill_rect_x[fill_rect_count] = x;
        fill_rect_y[fill_rect_count] = y;
        fill_rect_width[fill_rect_count] = width;
        fill_rect_height[fill_rect_count] = height;
    }
    ++fill_rect_count;
}

void ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    if (outline_rect_count < 2U)
    {
        outline_rect_x[outline_rect_count] = x;
        outline_rect_y[outline_rect_count] = y;
        outline_rect_width[outline_rect_count] = width;
        outline_rect_height[outline_rect_count] = height;
    }
    ++outline_rect_count;
}

void ssd1306_clear_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    (void)width;
    (void)height;
    if (clear_rect_count < 10U)
    {
        clear_rect_x[clear_rect_count] = x;
        clear_rect_y[clear_rect_count] = y;
    }
    ++clear_rect_count;
}

void ssd1306_draw_rect_inverted(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height)
{
    (void)width;
    (void)height;
    if (inverted_rect_count < 10U)
    {
        inverted_rect_x[inverted_rect_count] = x;
        inverted_rect_y[inverted_rect_count] = y;
    }
    ++inverted_rect_count;
}

void ssd1306_draw_hline(uint8_t x, uint8_t y, uint8_t width)
{
    (void)x;
    (void)y;
    (void)width;
    ++hline_count;
}

void ssd1306_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    ++line_count;
}

void ssd1306_invert(void)
{
    frame_inverted = !frame_inverted;
}

void ssd1306_draw_circle(uint8_t x, uint8_t y, bool filled)
{
    if (circle_count < 6U)
    {
        circle_x[circle_count] = x;
        circle_y[circle_count] = y;
        circle_filled[circle_count] = filled;
        ++circle_count;
    }
}

void ssd1306_draw_circle_inverted(uint8_t x, uint8_t y, bool filled)
{
    ssd1306_draw_circle(x, y, filled);
}

uint16_t ssd1306_text_width(const char *text)
{
    uint16_t width = 0U;
    const unsigned char *bytes = (const unsigned char *)text;
    while (*bytes != '\0')
    {
        if ((bytes[0] == 0xC2U) && (bytes[1] == 0xB0U)) ++bytes;
        ++bytes;
        width = (uint16_t)(width + 6U);
    }
    return width;
}

bool ssd1306_update(void)
{
    ++oled_update_count;
    return fake_oled_update_ok;
}

bool sensor_view_service_snapshot(
    uint32_t now_ms,
    sensor_view_snapshot_t *snapshot)
{
    (void)now_ms;
    *snapshot = (sensor_view_snapshot_t){
        .sensors = {
        .environment = {
            .quality = environment_valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_UNMEASURED,
            .temperature_c_x10 = environment_temperature,
            .humidity_pct_x10 = environment_humidity,
        },
        .ride = {
            .quality = ride_valid ? SENSOR_QUALITY_VALID : SENSOR_QUALITY_UNMEASURED,
            .kmh_x10 = ride_kmh_x10,
            .distance_mm = ride_distance_mm,
        },
        },
    };
    return true;
}

int main(void)
{
    CHECK(speed_level_from_kmh_x10(false, 316U) == 0U);
    CHECK(speed_level_from_kmh_x10(true, 0U) == 0U);
    CHECK(speed_level_from_kmh_x10(true, 1U) == 1U);
    CHECK(speed_level_from_kmh_x10(true, 95U) == 1U);
    CHECK(speed_level_from_kmh_x10(true, 96U) == 2U);
    CHECK(speed_level_from_kmh_x10(true, 123U) == 2U);
    CHECK(speed_level_from_kmh_x10(true, 124U) == 3U);
    CHECK(speed_level_from_kmh_x10(true, 150U) == 3U);
    CHECK(speed_level_from_kmh_x10(true, 151U) == 4U);
    CHECK(speed_level_from_kmh_x10(true, 178U) == 4U);
    CHECK(speed_level_from_kmh_x10(true, 179U) == 5U);
    CHECK(speed_level_from_kmh_x10(true, 205U) == 5U);
    CHECK(speed_level_from_kmh_x10(true, 206U) == 6U);
    CHECK(speed_level_from_kmh_x10(true, 233U) == 6U);
    CHECK(speed_level_from_kmh_x10(true, 234U) == 7U);
    CHECK(speed_level_from_kmh_x10(true, 260U) == 7U);
    CHECK(speed_level_from_kmh_x10(true, 261U) == 8U);
    CHECK(speed_level_from_kmh_x10(true, 288U) == 8U);
    CHECK(speed_level_from_kmh_x10(true, 289U) == 9U);
    CHECK(speed_level_from_kmh_x10(true, 315U) == 9U);
    CHECK(speed_level_from_kmh_x10(true, 316U) == 10U);
    CHECK(speed_level_from_kmh_x10(true, 426U) == 10U);

    I2C_HandleTypeDef i2c = {0};
    display_service_init(&i2c);
    CHECK(display_service_is_ready());
    CHECK(oled_init_count == 1U);
    CHECK(oled_update_count == 1U);
    CHECK(drawn_count == 6U);
    CHECK(strcmp(drawn_lines[0], "NOSTOS") == 0);
    CHECK(strcmp(drawn_lines[1], "N/A km/h") == 0);
    CHECK(strcmp(drawn_lines[2], "N/A km") == 0);
    CHECK(strcmp(drawn_lines[3], "N/A \xC2\xB0" "C") == 0);
    CHECK(strcmp(drawn_lines[4], "N/A %") == 0);
    CHECK(strcmp(drawn_lines[5],
        "Have an amazing ride! Remember, safety first.") == 0);
    CHECK(drawn_x[0] == 0U && drawn_y[0] == 1U);
    CHECK(drawn_x[1] == 8 && drawn_y[1] == 12U);
    CHECK(drawn_x[2] == 78 && drawn_y[2] == 12U);
    CHECK(drawn_x[3] == 14 && drawn_y[3] == 23U);
    CHECK(drawn_x[4] == 81 && drawn_y[4] == 23U);
    CHECK(drawn_x[5] == 128 && drawn_y[5] == 39U);
    CHECK(inverted_text_count == 1U);
    CHECK(fill_rect_count == 2U);
    CHECK(fill_rect_x[0] == 0U && fill_rect_y[0] == 0U);
    CHECK(fill_rect_width[0] == 128U && fill_rect_height[0] == 10U);
    CHECK(fill_rect_x[1] == 63U && fill_rect_y[1] == 11U);
    CHECK(fill_rect_width[1] == 1U && fill_rect_height[1] == 21U);
    CHECK(outline_rect_count == 0U);
    CHECK(hline_count == 4U);
    CHECK(line_count == 0U);
    CHECK(!frame_inverted);
    CHECK(circle_count == 0U);
    CHECK(clear_rect_count == 0U);
    CHECK(inverted_rect_count == 10U);
    for (uint8_t index = 0U; index < 10U; ++index)
    {
        CHECK(inverted_rect_x[index] == (uint8_t)(58U + index * 7U));
        CHECK(inverted_rect_y[index] == 2U);
    }
    CHECK(!display_service_show_button_message(0U, MSG_SPEED_UP_REQUEST));
    CHECK(!display_service_show_button_message(1U, MSG_FALL_DETECTED));
    CHECK(display_service_show_button_message(1U, MSG_SPEED_UP_REQUEST));

    /* A remote ENVIRONMENT report must render even when local DHT is disabled. */
    environment_valid = true;
    environment_temperature = 253;
    environment_humidity = 610U;
    ride_valid = true;
    ride_kmh_x10 = 228U;
    ride_distance_mm = 123456U;
    fake_tick = 200U;
    display_service_process();
    CHECK(drawn_count == 7U);
    CHECK(strcmp(drawn_lines[1], "22.8km/h") == 0);
    CHECK(strcmp(drawn_lines[2], "0.123km") == 0);
    CHECK(strcmp(drawn_lines[3], "25.3\xC2\xB0" "C") == 0);
    CHECK(strcmp(drawn_lines[4], "61.0%") == 0);
    CHECK(strcmp(drawn_lines[5],
        "Have an amazing ride! Remember, safety first.") == 0);
    CHECK(drawn_x[1] == 8 && drawn_y[1] == 12U);
    CHECK(drawn_x[2] == 75 && drawn_y[2] == 12U);
    CHECK(drawn_x[3] == 14 && drawn_y[3] == 23U);
    CHECK(drawn_x[4] == 81 && drawn_y[4] == 23U);
    CHECK(drawn_x[5] == 126 && drawn_y[5] == 39U);
    CHECK(strcmp(drawn_lines[6], "FRONT SENT ACCELERATE") == 0);
    CHECK(drawn_x[6] == 0U && drawn_y[6] == 55U);
    CHECK(circle_count == 0U);
    CHECK(clear_rect_count == 6U);
    CHECK(inverted_rect_count == 4U);
    for (uint8_t index = 0U; index < 6U; ++index)
    {
        CHECK(clear_rect_x[index] == (uint8_t)(58U + index * 7U));
        CHECK(clear_rect_y[index] == 2U);
    }
    CHECK(inverted_rect_x[0] == 100U);

    /* Calibration screens replace the dashboard and show bounded progress. */
    display_service_set_calibration(DISPLAY_CALIBRATION_INIT, 0U, 0U);
    fake_tick = 400U;
    display_service_process();
    CHECK(drawn_count == 2U);
    CHECK(strcmp(drawn_lines[0], "CAL INIT") == 0);
    CHECK(strcmp(drawn_lines[1], "PREPARING SENSOR") == 0);
    CHECK(outline_rect_count == 0U);

    display_service_set_calibration(DISPLAY_CALIBRATION_RUNNING, 600U, 0U);
    fake_tick = 600U;
    display_service_process();
    CHECK(drawn_count == 4U);
    CHECK(strcmp(drawn_lines[0], "CALIBRATING") == 0);
    CHECK(strcmp(drawn_lines[1], "KEEP BIKE UPRIGHT") == 0);
    CHECK(strcmp(drawn_lines[2], "AND HOLD IT STILL") == 0);
    CHECK(strcmp(drawn_lines[3], "60%") == 0);
    CHECK(outline_rect_count == 1U);
    CHECK(outline_rect_x[0] == 4U && outline_rect_y[0] == 42U);
    CHECK(outline_rect_width[0] == 120U && outline_rect_height[0] == 10U);
    CHECK(fill_rect_count == 1U);
    CHECK(fill_rect_x[0] == 6U && fill_rect_y[0] == 44U);
    CHECK(fill_rect_width[0] == 69U && fill_rect_height[0] == 6U);

    display_service_set_calibration(DISPLAY_CALIBRATION_SUCCESS, 0U, 0U);
    fake_tick = 800U;
    display_service_process();
    CHECK(drawn_count == 3U);
    CHECK(strcmp(drawn_lines[0], "CAL OK") == 0);
    CHECK(strcmp(drawn_lines[1], "FALL DETECTION") == 0);
    CHECK(strcmp(drawn_lines[2], "READY") == 0);

    display_service_set_calibration(DISPLAY_CALIBRATION_REQUIRED, 0U, 0U);
    fake_tick = 1000U;
    display_service_process();
    CHECK(drawn_count == 4U);
    CHECK(strcmp(drawn_lines[0], "CAL REQUIRED") == 0);
    CHECK(strcmp(drawn_lines[1], "KEEP BIKE STILL") == 0);
    CHECK(strcmp(drawn_lines[2], "HOLD BUTTON 1") == 0);
    CHECK(strcmp(drawn_lines[3], "FOR 3 SECONDS") == 0);

    display_service_set_calibration(DISPLAY_CALIBRATION_HOLDING, 600U, 1800U);
    fake_tick = 1200U;
    display_service_process();
    CHECK(drawn_count == 3U);
    CHECK(strcmp(drawn_lines[0], "CAL REQUIRED") == 0);
    CHECK(strcmp(drawn_lines[1], "HOLD BUTTON 1") == 0);
    CHECK(strcmp(drawn_lines[2], "1.8s") == 0);
    CHECK(outline_rect_count == 1U);
    CHECK(fill_rect_count == 1U);
    CHECK(fill_rect_width[0] == 69U);

    /* Inputs clamp to full scale: 1000 permille and 3000 ms. */
    display_service_set_calibration(DISPLAY_CALIBRATION_HOLDING, 1200U, 5000U);
    fake_tick = 1400U;
    display_service_process();
    CHECK(strcmp(drawn_lines[2], "3.0s") == 0);
    CHECK(fill_rect_width[0] == 116U);

    /* FALL remains the absolute full-screen priority over calibration. */
    display_service_set_fall(true);
    fake_tick = 1600U;
    display_service_process();
    CHECK(drawn_count == 1U);
    CHECK(strcmp(drawn_lines[0], "FALL DETECTED!") == 0);
    CHECK(line_count == 6U);
    display_service_set_fall(false);
    display_service_set_calibration(DISPLAY_CALIBRATION_READY, 0U, 0U);

    /* The last active segment pulses from filled to outline. */
    fake_tick = 2000U;
    display_service_process();
    CHECK(clear_rect_count == 5U);
    CHECK(inverted_rect_count == 5U);
    CHECK(inverted_rect_x[0] == 93U);
    CHECK(drawn_x[5] == 124 && drawn_y[5] == 39U);

    /* A fall replaces the whole dashboard with a blinking full-screen alert. */
    display_service_set_fall(true);
    fake_tick = 2200U;
    display_service_process();
    CHECK(drawn_count == 1U);
    CHECK(strcmp(drawn_lines[0], "FALL DETECTED!") == 0);
    CHECK(drawn_x[0] == 22 && drawn_y[0] == 50U);
    CHECK(line_count == 6U);
    CHECK(fill_rect_count == 2U);
    CHECK(hline_count == 0U);
    CHECK(circle_count == 0U);
    CHECK(clear_rect_count == 0U);
    CHECK(inverted_rect_count == 0U);
    CHECK(frame_inverted);

    fake_tick = 2400U;
    display_service_process();
    CHECK(drawn_count == 1U);
    CHECK(line_count == 6U);
    CHECK(!frame_inverted);

    environment_valid = false;
    ride_valid = false;
    display_service_set_fall(false);
    CHECK(display_service_show_button_message(2U, MSG_SPEED_DOWN_REQUEST));
    fake_tick = 2600U;
    display_service_process();
    CHECK(strcmp(drawn_lines[1], "N/A km/h") == 0);
    CHECK(strcmp(drawn_lines[2], "N/A km") == 0);
    CHECK(strcmp(drawn_lines[3], "N/A \xC2\xB0" "C") == 0);
    CHECK(strcmp(drawn_lines[4], "N/A %") == 0);
    CHECK(strcmp(drawn_lines[5],
        "Have an amazing ride! Remember, safety first.") == 0);
    CHECK(drawn_x[5] == 122 && drawn_y[5] == 39U);
    CHECK(strcmp(drawn_lines[6], "REAR SENT DECEL") == 0);
    CHECK(clear_rect_count == 0U);
    CHECK(inverted_rect_count == 10U);

    fake_oled_update_ok = false;
    CHECK(display_service_show_button_message(3U, MSG_STOP_REQUEST));
    fake_tick = 2800U;
    display_service_process();
    CHECK(strcmp(drawn_lines[6], "CENTER SENT STOP") == 0);
    CHECK(!display_service_is_ready());
    uint32_t retry_start = 2800U;

    fake_oled_init_ok = true;
    fake_oled_update_ok = true;
    fake_tick = retry_start + 1999U;
    display_service_process();
    CHECK(oled_init_count == 1U);
    fake_tick = retry_start + 2000U;
    display_service_process();
    CHECK(oled_init_count == 2U);
    CHECK(display_service_is_ready());

    puts("display_service tests passed");
    return 0;
}
