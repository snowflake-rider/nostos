#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DISPLAY_CALIBRATION_READY = 0,
    DISPLAY_CALIBRATION_INIT,
    DISPLAY_CALIBRATION_RUNNING,
    DISPLAY_CALIBRATION_SUCCESS,
    DISPLAY_CALIBRATION_REQUIRED,
    DISPLAY_CALIBRATION_HOLDING,
} display_calibration_state_t;

void display_service_init(I2C_HandleTypeDef *i2c);
void display_service_process(void);
bool display_service_is_ready(void);
/* Pure speed classification used by the five-circle dashboard indicator. */
uint8_t speed_level_from_kmh_x10(bool valid, uint16_t kmh_x10);
/* Displays whether any accepted local/remote fall incident is active. */
void display_service_set_fall(bool active);
/* Selects the full-screen calibration UI. READY restores the dashboard. */
void display_service_set_calibration(
    display_calibration_state_t state,
    uint16_t progress_permille,
    uint32_t hold_elapsed_ms);
/* Records the latest accepted button request for the bottom dashboard row. */
bool display_service_show_button_message(uint8_t sender_id, uint8_t type);

#endif /* DISPLAY_SERVICE_H */
