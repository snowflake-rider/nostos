/* Adapted from snowflake-rider/stm32-project commit 940ff240. */
#include "ssd1306.h"

#include "app_config.h"

#include <stddef.h>
#include <string.h>

#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U
#define OLED_ADDRESS ((uint16_t)(SSD1306_I2C_ADDRESS << 1U))
#define OLED_TIMEOUT_MS 20U

static I2C_HandleTypeDef *oled_i2c;
static uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8U];

static bool send_command(uint8_t value)
{
    uint8_t data[2] = {0x00U, value};
    return HAL_I2C_Master_Transmit(
        oled_i2c,
        OLED_ADDRESS,
        data,
        (uint16_t)sizeof(data),
        OLED_TIMEOUT_MS
    ) == HAL_OK;
}

/* 화면에 필요한 5x7 ASCII 글리프. 각 바이트는 한 세로 열입니다. */
static const uint8_t *glyph(char c)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t dash[5] = {8, 8, 8, 8, 8};
    static const uint8_t dot[5] = {0, 0, 0, 0x60, 0x60};
    static const uint8_t colon[5] = {0, 0x36, 0x36, 0, 0};
    static const uint8_t percent[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
    static const uint8_t digit[10][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0, 0x42, 0x7F, 0x40, 0},
        {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}
    };
    static const uint8_t upper[26][5] = {
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
        {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
        {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
        {0, 0x41, 0x7F, 0x41, 0}, {0x20, 0x40, 0x41, 0x3F, 0x01},
        {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}
    };

    if ((c >= '0') && (c <= '9')) return digit[(unsigned)(c - '0')];
    if ((c >= 'A') && (c <= 'Z')) return upper[(unsigned)(c - 'A')];
    if (c == '-') return dash;
    if (c == '.') return dot;
    if (c == ':') return colon;
    if (c == '%') return percent;
    return blank;
}

bool ssd1306_init(I2C_HandleTypeDef *i2c)
{
    static const uint8_t init[] = {
        0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
        0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
        0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
        0x40, 0x8D, 0x14, 0xAF
    };

    if (i2c == NULL) return false;
    oled_i2c = i2c;
    HAL_Delay(20U);
    for (size_t i = 0U; i < sizeof(init); ++i)
    {
        if (!send_command(init[i])) return false;
    }
    ssd1306_clear();
    return ssd1306_update();
}

void ssd1306_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text)
{
    if (text == NULL) return;
    while ((*text != '\0') && ((uint16_t)x + 5U < OLED_WIDTH))
    {
        const uint8_t *shape = glyph(*text++);
        for (uint8_t column = 0U; column < 5U; ++column)
        {
            for (uint8_t row = 0U; row < 7U; ++row)
            {
                uint16_t pixel_y = (uint16_t)y + row;
                if (((shape[column] & (uint8_t)(1U << row)) != 0U) &&
                    (pixel_y < OLED_HEIGHT))
                {
                    framebuffer[(pixel_y / 8U) * OLED_WIDTH + x + column] |=
                        (uint8_t)(1U << (pixel_y & 7U));
                }
            }
        }
        x = (uint8_t)(x + 6U);
    }
}

bool ssd1306_update(void)
{
    if (oled_i2c == NULL) return false;
    for (uint8_t page = 0U; page < 8U; ++page)
    {
        if (!send_command((uint8_t)(0xB0U + page)) ||
            !send_command(0x00U) || !send_command(0x10U))
        {
            return false;
        }
        for (uint8_t offset = 0U; offset < OLED_WIDTH; offset += 16U)
        {
            uint8_t data[17];
            data[0] = 0x40U;
            memcpy(
                &data[1],
                &framebuffer[(uint16_t)page * OLED_WIDTH + offset],
                16U
            );
            if (HAL_I2C_Master_Transmit(
                    oled_i2c,
                    OLED_ADDRESS,
                    data,
                    (uint16_t)sizeof(data),
                    OLED_TIMEOUT_MS
                ) != HAL_OK)
            {
                return false;
            }
        }
    }
    return true;
}
