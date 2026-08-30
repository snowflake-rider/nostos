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
static const uint8_t *glyph(uint8_t c)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t dash[5] = {8, 8, 8, 8, 8};
    static const uint8_t dot[5] = {0, 0, 0, 0x60, 0x60};
    static const uint8_t colon[5] = {0, 0x36, 0x36, 0, 0};
    static const uint8_t percent[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
    static const uint8_t slash[5] = {0x40, 0x30, 0x0C, 0x03, 0};
    static const uint8_t degree[5] = {0x06, 0x09, 0x09, 0x06, 0};
    static const uint8_t exclamation[5] = {0, 0, 0x5F, 0, 0};
    static const uint8_t comma[5] = {0, 0x50, 0x30, 0, 0};
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
    static const uint8_t lower[26][5] = {
        {0x20, 0x54, 0x54, 0x54, 0x78}, {0x7F, 0x48, 0x44, 0x44, 0x38},
        {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
        {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02},
        {0x0C, 0x52, 0x52, 0x52, 0x3E}, {0x7F, 0x08, 0x04, 0x04, 0x78},
        {0, 0x44, 0x7D, 0x40, 0}, {0x20, 0x40, 0x44, 0x3D, 0},
        {0x7F, 0x10, 0x28, 0x44, 0}, {0, 0x41, 0x7F, 0x40, 0},
        {0x7C, 0x04, 0x18, 0x04, 0x78}, {0x7C, 0x08, 0x04, 0x04, 0x78},
        {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
        {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08},
        {0x48, 0x54, 0x54, 0x54, 0x20}, {0x04, 0x3F, 0x44, 0x40, 0x20},
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44},
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, {0x44, 0x64, 0x54, 0x4C, 0x44}
    };

    if ((c >= '0') && (c <= '9')) return digit[(unsigned)(c - '0')];
    if ((c >= 'A') && (c <= 'Z')) return upper[(unsigned)(c - 'A')];
    if ((c >= 'a') && (c <= 'z')) return lower[(unsigned)(c - 'a')];
    if (c == '-') return dash;
    if (c == '.') return dot;
    if (c == ':') return colon;
    if (c == '%') return percent;
    if (c == '/') return slash;
    if (c == '!') return exclamation;
    if (c == ',') return comma;
    if (c == 0xB0U) return degree;
    return blank;
}

static uint8_t take_glyph_code(const char **text)
{
    const uint8_t *bytes = (const uint8_t *)*text;
    if ((bytes[0] == 0xC2U) && (bytes[1] == 0xB0U))
    {
        *text += 2;
        return 0xB0U;
    }
    ++(*text);
    return bytes[0];
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

static void set_pixel(uint16_t x, uint16_t y, bool on)
{
    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) return;

    uint8_t *cell = &framebuffer[(y / 8U) * OLED_WIDTH + x];
    uint8_t mask = (uint8_t)(1U << (y & 7U));
    if (on) *cell |= mask;
    else *cell &= (uint8_t)~mask;
}

static void set_pixel_clipped(int32_t x, int32_t y, bool on)
{
    if ((x < 0) || (x >= (int32_t)OLED_WIDTH) ||
        (y < 0) || (y >= (int32_t)OLED_HEIGHT))
    {
        return;
    }
    set_pixel((uint16_t)x, (uint16_t)y, on);
}

static void fill_rect(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    bool on
)
{
    for (uint16_t row = 0U; row < height; ++row)
    {
        for (uint16_t column = 0U; column < width; ++column)
        {
            set_pixel((uint16_t)x + column, (uint16_t)y + row, on);
        }
    }
}

void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    fill_rect(x, y, width, height, true);
}

void ssd1306_clear_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    fill_rect(x, y, width, height, false);
}

void ssd1306_draw_hline(uint8_t x, uint8_t y, uint8_t width)
{
    for (uint16_t column = 0U; column < width; ++column)
    {
        set_pixel((uint16_t)x + column, y, true);
    }
}

void ssd1306_draw_line(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
)
{
    int32_t x = x0;
    int32_t y = y0;
    const int32_t end_x = x1;
    const int32_t end_y = y1;
    const int32_t delta_x = end_x >= x ? end_x - x : x - end_x;
    const int32_t step_x = x < end_x ? 1 : -1;
    const int32_t delta_y_abs = end_y >= y ? end_y - y : y - end_y;
    const int32_t delta_y = -delta_y_abs;
    const int32_t step_y = y < end_y ? 1 : -1;
    int32_t error = delta_x + delta_y;

    for (;;)
    {
        set_pixel_clipped(x, y, true);
        if ((x == end_x) && (y == end_y)) break;

        const int32_t doubled_error = error * 2;
        if (doubled_error >= delta_y)
        {
            error += delta_y;
            x += step_x;
        }
        if (doubled_error <= delta_x)
        {
            error += delta_x;
            y += step_y;
        }
    }
}

static void draw_rect(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    bool on
)
{
    if ((width == 0U) || (height == 0U)) return;

    uint16_t bottom = (uint16_t)y + height - 1U;
    uint16_t right = (uint16_t)x + width - 1U;
    for (uint16_t column = 0U; column < width; ++column)
    {
        set_pixel((uint16_t)x + column, y, on);
        if (height > 1U) set_pixel((uint16_t)x + column, bottom, on);
    }
    for (uint16_t row = 1U; (row + 1U) < height; ++row)
    {
        set_pixel(x, (uint16_t)y + row, on);
        if (width > 1U) set_pixel(right, (uint16_t)y + row, on);
    }
}

void ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    draw_rect(x, y, width, height, true);
}

void ssd1306_draw_rect_inverted(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height
)
{
    draw_rect(x, y, width, height, false);
}

static void draw_circle(uint8_t x, uint8_t y, bool filled, bool inverted)
{
    static const uint8_t outline[5] = {0x0EU, 0x11U, 0x11U, 0x11U, 0x0EU};
    static const uint8_t solid[5] = {0x0EU, 0x1FU, 0x1FU, 0x1FU, 0x0EU};
    const uint8_t *shape = filled ? solid : outline;

    for (uint8_t column = 0U; column < 5U; ++column)
    {
        for (uint8_t row = 0U; row < 5U; ++row)
        {
            bool shape_on =
                (shape[column] & (uint8_t)(1U << row)) != 0U;
            set_pixel(
                (uint16_t)x + column,
                (uint16_t)y + row,
                inverted ? !shape_on : shape_on
            );
        }
    }
}

void ssd1306_draw_circle(uint8_t x, uint8_t y, bool filled)
{
    draw_circle(x, y, filled, false);
}

void ssd1306_draw_circle_inverted(uint8_t x, uint8_t y, bool filled)
{
    draw_circle(x, y, filled, true);
}

uint16_t ssd1306_text_width(const char *text)
{
    if (text == NULL) return 0U;

    uint16_t width = 0U;
    while ((*text != '\0') && (width <= (UINT16_MAX - 6U)))
    {
        (void)take_glyph_code(&text);
        width = (uint16_t)(width + 6U);
    }
    return width;
}

static void draw_text(int16_t x, uint8_t y, const char *text, bool inverted)
{
    if (text == NULL) return;
    int32_t cursor_x = x;
    while ((*text != '\0') && (cursor_x < (int32_t)OLED_WIDTH))
    {
        const uint8_t *shape = glyph(take_glyph_code(&text));
        for (uint8_t column = 0U; column < 6U; ++column)
        {
            for (uint8_t row = 0U; row < 8U; ++row)
            {
                bool glyph_on = (column < 5U) && (row < 7U) &&
                    ((shape[column] & (uint8_t)(1U << row)) != 0U);
                set_pixel_clipped(
                    cursor_x + column,
                    (int32_t)y + row,
                    inverted ? !glyph_on : glyph_on
                );
            }
        }
        cursor_x += 6;
    }
}

void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text)
{
    draw_text(x, y, text, false);
}

void ssd1306_draw_text_scrolled(int16_t x, uint8_t y, const char *text)
{
    draw_text(x, y, text, false);
}

void ssd1306_draw_text_inverted(uint8_t x, uint8_t y, const char *text)
{
    draw_text(x, y, text, true);
}

void ssd1306_invert(void)
{
    for (size_t i = 0U; i < sizeof(framebuffer); ++i)
    {
        framebuffer[i] = (uint8_t)~framebuffer[i];
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
