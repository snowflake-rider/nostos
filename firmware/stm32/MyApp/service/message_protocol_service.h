#ifndef MESSAGE_PROTOCOL_SERVICE_H
#define MESSAGE_PROTOCOL_SERVICE_H

#include "nostos_protocol.h"
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
    uint32_t duplicates;
    uint32_t overflows;
    uint32_t state_updates;
    uint32_t pace_requests;
    uint32_t stop_requests;
    uint32_t stop_acks;
    uint32_t stop_ack_matches;
    uint32_t stop_ack_ignored;
    uint32_t transmitted;
    uint32_t transmit_failures;
    nostos_result_t last_protocol_result;
} message_protocol_stats_t;

/* Starts the paired UART link. STOP acceptance state is RAM-only and is reset
 * here; there is no HELLO/READY handshake or persistent boot/session state. */
message_protocol_result_t message_protocol_service_boot(
    UART_HandleTypeDef *uart,
    vs1003b_status_t audio_status);
bool message_protocol_service_is_ready(void);
const message_protocol_stats_t *message_protocol_service_stats(void);

/* Called by the HAL UART ISR. Parsing and hardware work stay in process(). */
void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms);
void message_protocol_service_rx_error_isr(void);
/* Clears received/output state only. A local STOP awaiting ESP acceptance and
 * its request-id sequence survive; boot() is the reboot-state reset. */
void message_protocol_service_clear_pending(void);
void message_protocol_service_process(void);

/* Local STM32 producers use source 0; the paired ESP32 stamps its Node ID. */
message_protocol_result_t message_protocol_service_publish_event(
    uint8_t event_type);
message_protocol_result_t message_protocol_service_publish_ride(
    bool sensor_valid,
    uint16_t speed_x10_kmh,
    uint32_t trip_distance_m);
message_protocol_result_t message_protocol_service_publish_environment(
    bool sensor_valid,
    int16_t temperature_x10_c,
    uint16_t humidity_x10_pct);

#if defined(MESSAGE_PROTOCOL_TEST_PLATFORM_H)
void message_protocol_service_test_set_next_local_request_id(
    uint32_t request_id);
#endif

#endif /* MESSAGE_PROTOCOL_SERVICE_H */
