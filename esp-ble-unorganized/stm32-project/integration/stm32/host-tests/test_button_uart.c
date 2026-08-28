#include "button.h"
#include "main.h"
#include "uart_service.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static UART_HandleTypeDef uart1 = {1U};
static UART_HandleTypeDef uart2 = {2U};
static unsigned sent;
static unsigned traced;
static uint8_t last_byte;
static uint8_t trace_byte;
static HAL_StatusTypeDef transmit_result;
static HAL_StatusTypeDef trace_result;

uint32_t HAL_GetTick(void) { return tick; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->input & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size)
{
    CHECK(uart == &uart1 && data != NULL && size == 1U);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, const uint8_t *data,
                                 uint16_t size, uint32_t timeout)
{
    CHECK(data != NULL && size == 1U && timeout == 10U);
    if (uart == &uart2) {
        ++traced;
        trace_byte = data[0];
        return trace_result;
    }
    CHECK(uart == &uart1 && uart->instance == 1U);
    ++sent;
    last_byte = data[0];
    return transmit_result;
}
static void reset_at(uint32_t now)
{
    host_gpio_a.input = host_gpio_b.input = host_gpio_c.input = UINT16_MAX;
    tick = now;
    sent = 0;
    traced = 0;
    last_byte = 0;
    trace_byte = 0;
    transmit_result = HAL_OK;
    trace_result = HAL_OK;
    button_init();
    CHECK(uart_service_init(&uart1) == HAL_OK);
}
static message_type_t poll_after(uint32_t delta)
{
    tick += delta;
    message_type_t event = button_get_message();
    if (event != MSG_NONE) (void)uart_service_send_message(event);
    return event;
}
static void pb6_press_once(void)
{
    reset_at(100U);
    CHECK(poll_after(0U) == MSG_NONE);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(29U) == MSG_NONE);
    CHECK(poll_after(1U) == MSG_STOP_REQUEST);
    CHECK(sent == 1U && last_byte == 0x13U && uart_service_get_tx_count() == 1U);
    CHECK(poll_after(1000U) == MSG_NONE && sent == 1U);
    host_gpio_b.input |= GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_NONE);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_STOP_REQUEST);
    CHECK(sent == 2U && last_byte == 0x13U);
}
static void bounce_and_boot_held(void)
{
    reset_at(0U);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    host_gpio_b.input |= GPIO_PIN_6;
    CHECK(poll_after(10U) == MSG_NONE);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(10U) == MSG_NONE);
    CHECK(poll_after(29U) == MSG_NONE);
    CHECK(poll_after(1U) == MSG_STOP_REQUEST && sent == 1U);

    reset_at(0U);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    button_init(); /* Already held at boot: require release and a new press. */
    CHECK(poll_after(1000U) == MSG_NONE && sent == 0U);
    host_gpio_b.input |= GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_NONE);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_STOP_REQUEST && sent == 1U);
}
static void wraparound_and_transport_failure(void)
{
    reset_at(UINT32_MAX - 10U);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(29U) == MSG_NONE);
    transmit_result = HAL_TIMEOUT;
    CHECK(poll_after(1U) == MSG_STOP_REQUEST);
    CHECK(sent == 1U && uart_service_get_tx_count() == 0U);
    CHECK(uart_service_get_status() == HAL_TIMEOUT);
    CHECK(poll_after(1000U) == MSG_NONE && sent == 1U);
}
static void four_button_mapping_and_single_press(void)
{
    GPIO_TypeDef *ports[] = {GPIOB, GPIOB, GPIOA, GPIOC};
    const uint16_t pins[] = {GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_8, GPIO_PIN_7};
    const message_type_t events[] = {MSG_SPEED_UP_REQUEST, MSG_SPEED_DOWN_REQUEST,
                                    MSG_SAFETY_REMINDER, MSG_STOP_REQUEST};
    const uint8_t bytes[] = {0x11U, 0x10U, 0x12U, 0x13U};
    for (size_t i = 0; i < 4U; ++i) {
        reset_at(100U);
        ports[i]->input &= (uint16_t)~pins[i];
        CHECK(poll_after(0U) == MSG_NONE);
        CHECK(poll_after(29U) == MSG_NONE);
        CHECK(poll_after(1U) == events[i]);
        CHECK(sent == 1U && last_byte == bytes[i]);
        CHECK(poll_after(1000U) == MSG_NONE && sent == 1U);
        ports[i]->input |= pins[i];
        CHECK(poll_after(0U) == MSG_NONE);
        CHECK(poll_after(30U) == MSG_NONE && sent == 1U);
        ports[i]->input &= (uint16_t)~pins[i];
        CHECK(poll_after(0U) == MSG_NONE);
        CHECK(poll_after(30U) == events[i]);
        CHECK(sent == 2U && last_byte == bytes[i]);
    }
}
static void usb_trace_does_not_change_transport_result(void)
{
    reset_at(0U);
    uart_service_set_tx_trace(&uart2);
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_OK);
    CHECK(sent == 1U && traced == 1U && last_byte == 0x13U && trace_byte == 0x13U);
    CHECK(uart_service_get_tx_count() == 1U);
    trace_result = HAL_TIMEOUT;
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_OK);
    CHECK(sent == 2U && traced == 2U && uart_service_get_tx_count() == 2U);
    transmit_result = HAL_TIMEOUT;
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_TIMEOUT);
    CHECK(sent == 3U && traced == 2U && uart_service_get_tx_count() == 2U);
    reset_at(0U); /* init disables the optional trace */
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_OK);
    CHECK(sent == 1U && traced == 0U);
    uart_service_set_tx_trace(&uart1); /* never duplicate on the data UART */
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_OK);
    CHECK(sent == 2U && traced == 0U);
}
static void all_event_ids_use_one_uart_byte(void)
{
    const uint8_t ids[] = {0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x30, 0x31};
    reset_at(0U);
    for (size_t i = 0; i < sizeof(ids); ++i) {
        CHECK(uart_service_send_message((message_type_t)ids[i]) == HAL_OK);
        CHECK(last_byte == ids[i] && sent == i + 1U);
        CHECK(uart_service_get_tx_count() == i + 1U);
    }
    CHECK(traced == 0U);
}
int main(void)
{
    pb6_press_once();
    bounce_and_boot_held();
    wraparound_and_transport_failure();
    four_button_mapping_and_single_press();
    usb_trace_does_not_change_transport_result();
    all_event_ids_use_one_uart_byte();
    puts("PASS PB6 active-low: 29/30ms debounce, one 0x13 byte via selected UART, hold/release/repress");
    puts("PASS bounce, boot-held suppression, tick wrap, transport failure");
    puts("PASS D4/PB5=0x11, D6/PB10=0x10, D7/PA8=0x12, D9/PC7=0x13: debounce, hold/release/repress");
    puts("HARDWARE_PIN_ROUTING_AND_MESH=NOT_TESTED");
    return 0;
}
