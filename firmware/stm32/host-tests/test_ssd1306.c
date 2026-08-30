#include "ssd1306.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_TRANSMISSIONS 128U
#define MAX_PAYLOAD 17U

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

typedef struct {
    uint16_t address;
    uint16_t size;
    uint8_t data[MAX_PAYLOAD];
} transmission_t;

static transmission_t transmissions[MAX_TRANSMISSIONS];
static size_t transmission_count;
static uint32_t delay_ms;

static void reset_transmissions(void)
{
    transmission_count = 0U;
    memset(transmissions, 0, sizeof(transmissions));
}

static uint8_t framebuffer_byte(uint8_t page, uint8_t x)
{
    const size_t page_start = (size_t)page * 11U;
    const size_t chunk = (size_t)x / 16U;
    const size_t byte_index = (size_t)x % 16U;
    return transmissions[page_start + 3U + chunk].data[1U + byte_index];
}

void HAL_Delay(uint32_t delay)
{
    delay_ms += delay;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(
    I2C_HandleTypeDef *i2c,
    uint16_t address,
    uint8_t *data,
    uint16_t size,
    uint32_t timeout
)
{
    (void)i2c;
    (void)timeout;
    if ((transmission_count >= MAX_TRANSMISSIONS) ||
        (size > MAX_PAYLOAD))
    {
        return HAL_ERROR;
    }

    transmission_t *tx = &transmissions[transmission_count++];
    tx->address = address;
    tx->size = size;
    memcpy(tx->data, data, size);
    return HAL_OK;
}

int main(void)
{
    I2C_HandleTypeDef i2c = {0};
    CHECK(ssd1306_init(&i2c));
    CHECK(delay_ms == 20U);
    CHECK(transmission_count == 116U);
    CHECK(transmissions[0].address == 0x78U);
    CHECK(transmissions[1].size == 2U);
    CHECK(transmissions[1].data[0] == 0x00U);
    CHECK(transmissions[1].data[1] == 0x20U);
    CHECK(transmissions[2].data[1] == 0x00U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_draw_text(0U, 0U, "N/A");
    CHECK(ssd1306_update());

    CHECK(transmission_count == 88U);
    CHECK(transmissions[3].size == 17U);
    CHECK(transmissions[3].data[0] == 0x40U);
    CHECK(transmissions[3].data[7] == 0x40U);
    CHECK(transmissions[3].data[8] == 0x30U);
    CHECK(transmissions[3].data[9] == 0x0CU);
    CHECK(transmissions[3].data[10] == 0x03U);
    CHECK(transmissions[3].data[11] == 0x00U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_fill_rect(0U, 0U, 2U, 2U);
    ssd1306_draw_hline(2U, 1U, 2U);
    ssd1306_draw_rect(4U, 0U, 3U, 3U);
    ssd1306_draw_text_inverted(8U, 0U, "A");
    ssd1306_draw_circle(16U, 0U, true);
    ssd1306_draw_circle(22U, 0U, false);
    ssd1306_draw_text(28U, 0U, "\xC2\xB0");
    CHECK(ssd1306_update());

    CHECK(transmissions[3].data[1] == 0x03U);
    CHECK(transmissions[3].data[2] == 0x03U);
    CHECK(transmissions[3].data[3] == 0x02U);
    CHECK(transmissions[3].data[4] == 0x02U);
    CHECK(transmissions[3].data[5] == 0x07U);
    CHECK(transmissions[3].data[6] == 0x05U);
    CHECK(transmissions[3].data[7] == 0x07U);
    CHECK(transmissions[3].data[9] == 0x81U);
    CHECK(transmissions[3].data[10] == 0xEEU);
    CHECK(transmissions[3].data[14] == 0xFFU);
    CHECK(transmissions[4].data[1] == 0x0EU);
    CHECK(transmissions[4].data[2] == 0x1FU);
    CHECK(transmissions[4].data[5] == 0x0EU);
    CHECK(transmissions[4].data[7] == 0x0EU);
    CHECK(transmissions[4].data[8] == 0x11U);
    CHECK(transmissions[4].data[11] == 0x0EU);
    CHECK(transmissions[4].data[13] == 0x06U);
    CHECK(transmissions[4].data[14] == 0x09U);
    CHECK(transmissions[4].data[16] == 0x06U);
    CHECK(ssd1306_text_width("25.3\xC2\xB0" "C") == 36U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_fill_rect(0U, 0U, 16U, 8U);
    ssd1306_draw_circle_inverted(0U, 0U, true);
    ssd1306_draw_circle_inverted(6U, 0U, false);
    CHECK(ssd1306_update());
    CHECK(framebuffer_byte(0U, 0U) == 0xF1U);
    CHECK(framebuffer_byte(0U, 1U) == 0xE0U);
    CHECK(framebuffer_byte(0U, 4U) == 0xF1U);
    CHECK(framebuffer_byte(0U, 5U) == 0xFFU);
    CHECK(framebuffer_byte(0U, 6U) == 0xF1U);
    CHECK(framebuffer_byte(0U, 7U) == 0xEEU);
    CHECK(framebuffer_byte(0U, 10U) == 0xF1U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_fill_rect(0U, 0U, 12U, 8U);
    ssd1306_clear_rect(0U, 0U, 5U, 5U);
    ssd1306_draw_rect_inverted(6U, 0U, 5U, 5U);
    CHECK(ssd1306_update());
    CHECK(framebuffer_byte(0U, 0U) == 0xE0U);
    CHECK(framebuffer_byte(0U, 4U) == 0xE0U);
    CHECK(framebuffer_byte(0U, 5U) == 0xFFU);
    CHECK(framebuffer_byte(0U, 6U) == 0xE0U);
    CHECK(framebuffer_byte(0U, 7U) == 0xEEU);
    CHECK(framebuffer_byte(0U, 10U) == 0xE0U);
    CHECK(framebuffer_byte(0U, 11U) == 0xFFU);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_draw_text(0U, 0U, "a!,");
    CHECK(ssd1306_update());
    CHECK(framebuffer_byte(0U, 0U) == 0x20U);
    CHECK(framebuffer_byte(0U, 1U) == 0x54U);
    CHECK(framebuffer_byte(0U, 4U) == 0x78U);
    CHECK(framebuffer_byte(0U, 8U) == 0x5FU);
    CHECK(framebuffer_byte(0U, 13U) == 0x50U);
    CHECK(framebuffer_byte(0U, 14U) == 0x30U);
    CHECK(ssd1306_text_width(
        "Have an amazing ride! Remember, safety first.") == 270U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_draw_text_scrolled(-3, 0U, "A");
    ssd1306_draw_text_scrolled(125, 8U, "A");
    CHECK(ssd1306_update());
    CHECK(framebuffer_byte(0U, 0U) == 0x11U);
    CHECK(framebuffer_byte(0U, 1U) == 0x7EU);
    CHECK(framebuffer_byte(0U, 2U) == 0x00U);
    CHECK(framebuffer_byte(1U, 124U) == 0x00U);
    CHECK(framebuffer_byte(1U, 125U) == 0x7EU);
    CHECK(framebuffer_byte(1U, 126U) == 0x11U);
    CHECK(framebuffer_byte(1U, 127U) == 0x11U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_draw_line(0, 0, 7, 7);
    ssd1306_draw_line(14, 3, 10, 7);
    ssd1306_draw_line(-2, 0, 2, 0);
    CHECK(ssd1306_update());
    CHECK(framebuffer_byte(0U, 0U) == 0x01U);
    CHECK(framebuffer_byte(0U, 1U) == 0x03U);
    CHECK(framebuffer_byte(0U, 2U) == 0x05U);
    CHECK(framebuffer_byte(0U, 3U) == 0x08U);
    CHECK(framebuffer_byte(0U, 4U) == 0x10U);
    CHECK(framebuffer_byte(0U, 5U) == 0x20U);
    CHECK(framebuffer_byte(0U, 6U) == 0x40U);
    CHECK(framebuffer_byte(0U, 7U) == 0x80U);
    CHECK(framebuffer_byte(0U, 10U) == 0x80U);
    CHECK(framebuffer_byte(0U, 11U) == 0x40U);
    CHECK(framebuffer_byte(0U, 12U) == 0x20U);
    CHECK(framebuffer_byte(0U, 13U) == 0x10U);
    CHECK(framebuffer_byte(0U, 14U) == 0x08U);

    reset_transmissions();
    ssd1306_clear();
    ssd1306_draw_line(0, 0, 0, 0);
    ssd1306_invert();
    CHECK(ssd1306_update());
    CHECK(framebuffer_byte(0U, 0U) == 0xFEU);
    CHECK(framebuffer_byte(0U, 1U) == 0xFFU);
    CHECK(framebuffer_byte(7U, 127U) == 0xFFU);

    puts("ssd1306 tests passed");
    return 0;
}
