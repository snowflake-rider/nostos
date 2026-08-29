#ifndef DHT11_H
#define DHT11_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
} dht11_data_t;

bool dht11_init(GPIO_TypeDef *port, uint16_t pin);
bool dht11_read(dht11_data_t *data);

#endif /* DHT11_H */
