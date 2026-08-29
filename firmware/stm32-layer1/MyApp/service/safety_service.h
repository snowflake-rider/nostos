#ifndef SAFETY_SERVICE_H
#define SAFETY_SERVICE_H

#include "safety_detector.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool mpu_ready;
    bool mpu_data_valid;
    uint8_t mpu_address;
    uint32_t mpu_failure_count;
    bool distance_valid;
    float distance_cm;
    safety_event_t event;
    fall_state_t fall_state;
    uint32_t countdown_remaining_seconds;
} safety_service_status_t;

void safety_service_init(I2C_HandleTypeDef *i2c);
void safety_service_process(void);
const safety_service_status_t *safety_service_get_status(void);

#endif /* SAFETY_SERVICE_H */
