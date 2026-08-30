#include "uart_service.h"
#if NOSTOS_PROTOCOL_V2
#include "message_protocol_service.h"
#endif

#define UART_TRANSMIT_TIMEOUT_MS 10U

static UART_HandleTypeDef *message_uart = NULL;
static UART_HandleTypeDef *trace_uart = NULL;
static uint8_t receive_byte = 0U;
static volatile message_type_t pending_message = MSG_NONE;
static volatile bool message_pending = false;
static volatile HAL_StatusTypeDef uart_status = HAL_ERROR;
static volatile uint32_t tx_count = 0U;
static volatile uint32_t rx_count = 0U;
static volatile uint32_t invalid_count = 0U;
static volatile uint32_t dropped_count = 0U;

#if !NOSTOS_PROTOCOL_V2
static message_type_t uart_service_decode(uint8_t value)
{
    switch (value)
    {
        case MSG_NONE:
        case MSG_SPEED_DOWN_REQUEST:
        case MSG_SPEED_UP_REQUEST:
        case MSG_STOP_REQUEST:
        case MSG_FALL_DETECTED:
            return (message_type_t)value;

        default:
            return MSG_UNKNOWN;
    }
}
#endif

static void uart_service_restart_receive(void)
{
    if (message_uart != NULL)
    {
        uart_status = HAL_UART_Receive_IT(message_uart, &receive_byte, 1U);
    }
}

HAL_StatusTypeDef uart_service_init(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        uart_status = HAL_ERROR;
        return HAL_ERROR;
    }

    message_uart = huart;
    trace_uart = NULL;
    pending_message = MSG_NONE;
    message_pending = false;
    tx_count = 0U;
    rx_count = 0U;
    invalid_count = 0U;
    dropped_count = 0U;

    uart_service_restart_receive();
    return uart_status;
}

void uart_service_set_tx_trace(UART_HandleTypeDef *huart)
{
    trace_uart = (huart != message_uart) ? huart : NULL;
}

HAL_StatusTypeDef uart_service_send_message(message_type_t message)
{
    if ((message_uart == NULL) || (message == MSG_NONE) ||
        (message == MSG_UNKNOWN))
    {
        uart_status = HAL_ERROR;
        return HAL_ERROR;
    }

#if NOSTOS_PROTOCOL_V2
    uart_status = message_protocol_service_publish_event((uint8_t)message) ==
        MESSAGE_PROTOCOL_OK ? HAL_OK : HAL_ERROR;
    if(uart_status==HAL_OK) ++tx_count;
    return uart_status;
#else
    uint8_t transmit_byte = (uint8_t)message;
    uart_status = HAL_UART_Transmit(
        message_uart,
        &transmit_byte,
        1U,
        UART_TRANSMIT_TIMEOUT_MS
    );

    if (uart_status == HAL_OK)
    {
        ++tx_count;
        /* 진단 출력 실패가 실제 전송 결과/카운터를 바꾸거나 재전송하지 않게 합니다. */
        if (trace_uart != NULL)
        {
            (void)HAL_UART_Transmit(trace_uart, &transmit_byte, 1U,
                                    UART_TRANSMIT_TIMEOUT_MS);
        }
    }

    return uart_status;
#endif
}

bool uart_service_get_message(message_type_t *message)
{
    if ((message == NULL) || !message_pending)
    {
        return false;
    }

    __disable_irq();
    *message = pending_message;
    pending_message = MSG_NONE;
    message_pending = false;
    __enable_irq();

    return true;
}

void uart_service_clear_pending(void)
{
    __disable_irq();
    pending_message = MSG_NONE;
    message_pending = false;
    __enable_irq();
#if NOSTOS_PROTOCOL_V2
    message_protocol_service_clear_pending();
#endif
}

HAL_StatusTypeDef uart_service_get_status(void)
{
    return uart_status;
}

uint32_t uart_service_get_tx_count(void)
{
    return tx_count;
}

uint32_t uart_service_get_rx_count(void)
{
    return rx_count;
}

uint32_t uart_service_get_invalid_count(void)
{
    return invalid_count;
}

uint32_t uart_service_get_dropped_count(void)
{
    return dropped_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((message_uart != NULL) && (huart == message_uart))
    {
#if NOSTOS_PROTOCOL_V2
        message_protocol_service_rx_isr(receive_byte,HAL_GetTick());
#else
        message_type_t decoded_message = uart_service_decode(receive_byte);

        if (decoded_message == MSG_UNKNOWN)
        {
            ++invalid_count;
        }
        else if (decoded_message != MSG_NONE)
        {
            if (message_pending)
            {
                ++dropped_count;
            }
            else
            {
                pending_message = decoded_message;
                message_pending = true;
                ++rx_count;
            }
        }
#endif

        uart_service_restart_receive();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((message_uart != NULL) && (huart == message_uart))
    {
        uart_status = HAL_ERROR;
#if NOSTOS_PROTOCOL_V2
        message_protocol_service_rx_error_isr();
#endif
        uart_service_restart_receive();
    }
}
