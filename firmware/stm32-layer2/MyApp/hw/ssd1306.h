#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

bool ssd1306_init(I2C_HandleTypeDef *i2c);
void ssd1306_clear(void);
void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text);
bool ssd1306_update(void);

#endif /* SSD1306_H */
