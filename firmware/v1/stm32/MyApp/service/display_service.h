#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>

void display_service_init(I2C_HandleTypeDef *i2c);
void display_service_process(void);
bool display_service_is_ready(void);

#endif /* DISPLAY_SERVICE_H */
