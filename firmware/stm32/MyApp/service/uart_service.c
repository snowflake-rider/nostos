#include "uart_service.h"

#include "message_protocol_service.h"

static UART_HandleTypeDef *message_uart;
static uint8_t receive_byte;
static volatile HAL_StatusTypeDef uart_status = HAL_ERROR;
static volatile uint32_t tx_count;
static volatile uint32_t rx_count;
static volatile uint32_t invalid_count;
static volatile uint32_t dropped_count;

static void uart_service_restart_receive(void)
{
    if (message_uart != NULL) {
        uart_status = HAL_UART_Receive_IT(message_uart, &receive_byte, 1U);
    }
}

HAL_StatusTypeDef uart_service_init(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        uart_status = HAL_ERROR;
        return HAL_ERROR;
    }
    message_uart = huart;
    tx_count = 0U;
    rx_count = 0U;
    invalid_count = 0U;
    dropped_count = 0U;
    uart_service_restart_receive();
    return uart_status;
}

HAL_StatusTypeDef uart_service_send_message(message_type_t message)
{
    if (message_uart == NULL || message == MSG_NONE || message == MSG_UNKNOWN) {
        uart_status = HAL_ERROR;
        return HAL_ERROR;
    }
    uart_status = message_protocol_service_publish_event((uint8_t)message) ==
        MESSAGE_PROTOCOL_OK ? HAL_OK : HAL_ERROR;
    if (uart_status == HAL_OK) ++tx_count;
    return uart_status;
}

void uart_service_clear_pending(void)
{
    message_protocol_service_clear_pending();
}

HAL_StatusTypeDef uart_service_get_status(void) { return uart_status; }
uint32_t uart_service_get_tx_count(void) { return tx_count; }
uint32_t uart_service_get_rx_count(void) { return rx_count; }
uint32_t uart_service_get_invalid_count(void) { return invalid_count; }
uint32_t uart_service_get_dropped_count(void) { return dropped_count; }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (message_uart != NULL && huart == message_uart) {
        message_protocol_service_rx_isr(receive_byte, HAL_GetTick());
        ++rx_count;
        uart_service_restart_receive();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (message_uart != NULL && huart == message_uart) {
        uart_status = HAL_ERROR;
        ++invalid_count;
        message_protocol_service_rx_error_isr();
        uart_service_restart_receive();
    }
}
