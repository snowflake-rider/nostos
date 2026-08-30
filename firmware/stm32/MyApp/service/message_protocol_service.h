#ifndef MESSAGE_PROTOCOL_SERVICE_H
#define MESSAGE_PROTOCOL_SERVICE_H

#include "sensor_link.h"
#include "stm32f4xx_hal.h"
#include "vs1003b.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MESSAGE_PROTOCOL_OK = 0,
    MESSAGE_PROTOCOL_NOT_READY,
    MESSAGE_PROTOCOL_BAD_ARGUMENT,
    MESSAGE_PROTOCOL_BAD_VALUE,
    MESSAGE_PROTOCOL_IO_ERROR,
} message_protocol_result_t;

typedef struct {
    uint32_t received;
    uint32_t rejected;
    uint32_t overflows;
    uint32_t hello_sent;
    uint32_t hello_failures;
    uint32_t ready_received;
    uint32_t output_accepted;
    uint32_t output_duplicates;
    uint32_t output_rejected;
    uint32_t output_hardware_errors;
    uint32_t result_sent;
    uint32_t result_failures;
    sensor_link_result_t last_link_result;
} message_protocol_stats_t;

/* Starts the paired local UART link. HELLO repeats until READY is parsed. */
message_protocol_result_t message_protocol_service_boot(
    UART_HandleTypeDef *uart,
    vs1003b_status_t audio_status);
bool message_protocol_service_is_ready(void);
const message_protocol_stats_t *message_protocol_service_stats(void);

/* Called by the HAL UART ISR. Parsing and hardware work stay in process(). */
void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms);
void message_protocol_service_rx_error_isr(void);
void message_protocol_service_clear_pending(void);
void message_protocol_service_process(void);

/* STM32 producer frames. These never create an official NOSTOS packet here. */
message_protocol_result_t message_protocol_service_publish_event(
    uint8_t event_type);
message_protocol_result_t message_protocol_service_publish_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm);
message_protocol_result_t message_protocol_service_publish_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    uint8_t quality);

#endif /* MESSAGE_PROTOCOL_SERVICE_H */
