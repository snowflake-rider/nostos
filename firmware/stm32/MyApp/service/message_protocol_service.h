#ifndef MESSAGE_PROTOCOL_SERVICE_H
#define MESSAGE_PROTOCOL_SERVICE_H
#include "nostos_endpoint.h"
#include "vs1003b.h"
/* v2 service. Application owns UART ISR handoff and trusted session restore. */
nostos_result_t message_protocol_service_init(UART_HandleTypeDef *uart,
    uint8_t source_id, uint32_t session_id, vs1003b_status_t audio_status);
nostos_endpoint_t *message_protocol_service_endpoint(void);
typedef struct {
    uint32_t received, duplicates, rejected, overflows;
    uint32_t sensor_received, sensor_rejected;
    uint32_t hello_sent, hello_failures;
    uint32_t identities, identity_acks;
    uint32_t sessions_approved, control_rejected;
    nostos_result_t last_result;
} message_protocol_stats_t;
const message_protocol_stats_t *message_protocol_service_stats(void);
nostos_result_t message_protocol_service_receive(uint8_t byte, uint32_t received_ms);
void message_protocol_service_process(void);
/* Called by HAL Rx ISR; all parse/apply/output work is deferred to process(). */
void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms);
void message_protocol_service_rx_error_isr(void);
/* ISR 수신 링, 부분 프레임, 아직 재생하지 않은 요청 큐를 모두 비웁니다. */
void message_protocol_service_clear_pending(void);
nostos_result_t message_protocol_service_publish_event(uint8_t type);
nostos_result_t message_protocol_service_publish_ride(
    bool valid,
    uint16_t kmh_x10,
    uint32_t distance_mm);
nostos_result_t message_protocol_service_publish_environment(
    int16_t temperature_c_x10,
    uint16_t humidity_pct_x10,
    nostos_quality_t quality);
/* Prepare the local ESP32 UART link and request the provisioned source/session.
 * The NOSTOS endpoint remains unavailable until a valid local IDENTITY arrives. */
nostos_result_t message_protocol_service_boot(
    UART_HandleTypeDef *uart,
    vs1003b_status_t audio_status);
#endif
