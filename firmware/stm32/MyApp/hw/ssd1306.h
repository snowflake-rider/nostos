#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

bool ssd1306_init(I2C_HandleTypeDef *i2c);
void ssd1306_clear(void);
void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void ssd1306_clear_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void ssd1306_draw_rect_inverted(
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height
);
void ssd1306_draw_hline(uint8_t x, uint8_t y, uint8_t width);
void ssd1306_draw_line(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
);
void ssd1306_draw_circle(uint8_t x, uint8_t y, bool filled);
void ssd1306_draw_circle_inverted(uint8_t x, uint8_t y, bool filled);
uint16_t ssd1306_text_width(const char *text);
void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text);
void ssd1306_draw_text_scrolled(int16_t x, uint8_t y, const char *text);
void ssd1306_draw_text_inverted(uint8_t x, uint8_t y, const char *text);
void ssd1306_invert(void);
bool ssd1306_update(void);

#endif /* SSD1306_H */
