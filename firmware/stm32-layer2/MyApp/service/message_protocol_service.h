#ifndef MESSAGE_PROTOCOL_SERVICE_H
#define MESSAGE_PROTOCOL_SERVICE_H
#include "nostos_endpoint.h"
#include "vs1003b.h"
/* v2 service. Application owns UART ISR handoff and trusted session restore.
 * Must not share UART/output ownership with legacy message_service. */
nostos_result_t message_protocol_service_init(UART_HandleTypeDef *uart,
    uint8_t source_id, uint32_t session_id, vs1003b_status_t audio_status);
nostos_endpoint_t *message_protocol_service_endpoint(void);
typedef struct {
    uint32_t received, duplicates, rejected, overflows;
    nostos_result_t last_result;
} message_protocol_stats_t;
const message_protocol_stats_t *message_protocol_service_stats(void);
nostos_result_t message_protocol_service_receive(uint8_t byte, uint32_t received_ms);
void message_protocol_service_process(void);
/* Called by HAL Rx ISR; all parse/apply/output work is deferred to process(). */
void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms);
void message_protocol_service_rx_error_isr(void);
nostos_result_t message_protocol_service_publish_event(uint8_t type);
/* Deployment overrides this weak hook to restore an approved boot context.
 * Default returns NOT_READY. No insecure fixed epoch or auto-approval. */
nostos_result_t message_protocol_service_boot(UART_HandleTypeDef *uart, vs1003b_status_t audio_status);
#endif
