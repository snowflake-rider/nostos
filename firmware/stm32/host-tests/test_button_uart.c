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
static uint8_t *receive_target;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

uint32_t HAL_GetTick(void) { return tick; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->input & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size)
{
    CHECK(uart == &uart1 && data != NULL && size == 1U);
    receive_target = data;
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
static message_type_t press_and_release(GPIO_TypeDef *port, uint16_t pin)
{
    port->input &= (uint16_t)~pin;
    CHECK(poll_after(0U) == MSG_NONE);
    message_type_t event = poll_after(30U);
    port->input |= pin;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_NONE);
    return event;
}

static void prototype_buttons_use_new_mapping(void)
{
    GPIO_TypeDef *ports[] = {GPIOB, GPIOB, GPIOA};
    const uint16_t pins[] = {GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_8};
    const message_type_t events[] = {MSG_SPEED_UP_REQUEST,
                                    MSG_SPEED_DOWN_REQUEST,
                                    MSG_STOP_REQUEST};
    const uint8_t bytes[] = {0x11U, 0x10U, 0x13U};
    for (size_t i = 0U; i < 3U; ++i) {
        reset_at(100U);
        CHECK(press_and_release(ports[i], pins[i]) == events[i]);
        CHECK(sent == 1U && last_byte == bytes[i]);
        CHECK(!button_take_output_reset_request());
    }
}

static void btn1_reports_debounced_stable_state(void)
{
    reset_at(100U);
    CHECK(!button_is_pressed(BUTTON_ID_BTN1));
    CHECK(!button_is_pressed((button_id_t)99));

    GPIOB->input &= (uint16_t)~GPIO_PIN_5;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(!button_is_pressed(BUTTON_ID_BTN1));
    CHECK(poll_after(29U) == MSG_NONE);
    CHECK(!button_is_pressed(BUTTON_ID_BTN1));
    CHECK(poll_after(1U) == MSG_SPEED_UP_REQUEST);
    CHECK(button_is_pressed(BUTTON_ID_BTN1));

    CHECK(poll_after(1000U) == MSG_NONE);
    CHECK(button_is_pressed(BUTTON_ID_BTN1));

    GPIOB->input |= GPIO_PIN_5;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(button_is_pressed(BUTTON_ID_BTN1));
    CHECK(poll_after(29U) == MSG_NONE);
    CHECK(button_is_pressed(BUTTON_ID_BTN1));
    CHECK(poll_after(1U) == MSG_NONE);
    CHECK(!button_is_pressed(BUTTON_ID_BTN1));
}

static void btn4_requests_local_reset_once(void)
{
    reset_at(100U);
    CHECK(press_and_release(GPIOC, GPIO_PIN_7) == MSG_NONE);
    CHECK(sent == 0U);
    CHECK(button_take_output_reset_request());
    CHECK(!button_take_output_reset_request());
}

static void btn4_reset_wins_over_simultaneous_message(void)
{
    reset_at(100U);
    GPIOB->input &= (uint16_t)~GPIO_PIN_5;
    GPIOC->input &= (uint16_t)~GPIO_PIN_7;
    CHECK(poll_after(0U) == MSG_NONE);
    tick += 30U;
    CHECK(button_get_message() == MSG_SPEED_UP_REQUEST);
    CHECK(button_take_output_reset_request());
    CHECK(sent == 0U);
}

static void pending_uart_message_can_be_cleared(void)
{
    reset_at(100U);
    CHECK(receive_target != NULL);

    *receive_target = (uint8_t)MSG_SPEED_UP_REQUEST;
    HAL_UART_RxCpltCallback(&uart1);
    uart_service_clear_pending();

    message_type_t message = MSG_NONE;
    CHECK(!uart_service_get_message(&message));

    *receive_target = (uint8_t)MSG_STOP_REQUEST;
    HAL_UART_RxCpltCallback(&uart1);
    CHECK(uart_service_get_message(&message));
    CHECK(message == MSG_STOP_REQUEST);
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
int main(void)
{
    pb6_press_once();
    bounce_and_boot_held();
    wraparound_and_transport_failure();
    prototype_buttons_use_new_mapping();
    btn1_reports_debounced_stable_state();
    btn4_requests_local_reset_once();
    btn4_reset_wins_over_simultaneous_message();
    pending_uart_message_can_be_cleared();
    usb_trace_does_not_change_transport_result();
    puts("PASS PB6 active-low: 29/30ms debounce, one 0x13 byte via selected UART, hold/release/repress");
    puts("PASS BTN1=UP/0x11, BTN2=DOWN/0x10, BTN3=STOP/0x13, BTN4=local reset");
    puts("PASS BTN1 debounced stable press/hold/release state and invalid button ID");
    puts("PASS BTN4 reset is one-shot, wins over a simultaneous message, and clears pending UART RX");
    puts("PASS bounce, boot-held, tick wrap, and transport failure");
    puts("HARDWARE_PIN_ROUTING_AND_MESH=NOT_TESTED");
    return 0;
}
