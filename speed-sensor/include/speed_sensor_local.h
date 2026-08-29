#ifndef SPEED_SENSOR_LOCAL_H
#define SPEED_SENSOR_LOCAL_H

#include "xoss_csc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Prototype-only ESP32 -> STM32 UART body. It is not a NOSTOS Mesh packet:
 * source/session/sequence are deliberately absent and must be assigned by
 * the STM32 endpoint after decoding this local sample.
 *
 * The body is 9 bytes so the existing length+CRC UART framer can carry it.
 */
#define SPEED_SENSOR_LOCAL_PAYLOAD_SIZE 9U

bool speed_sensor_local_encode(const xoss_speed_sample_t *sample,
                               uint8_t *payload,
                               size_t capacity);
bool speed_sensor_local_decode(const uint8_t *payload,
                               size_t length,
                               xoss_speed_sample_t *sample);

#endif
