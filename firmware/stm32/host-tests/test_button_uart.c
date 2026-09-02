#include "button.h"
#include "main.h"
#include "message_protocol_service.h"
#include "uart_service.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static uint32_t tick;
static UART_HandleTypeDef uart1 = {1U};
static uint8_t *receive_target;
static unsigned published;
static uint8_t last_published;
static message_protocol_result_t publish_result;
static unsigned protocol_rx_count;
static uint8_t protocol_rx_byte;
static unsigned protocol_clear_count;

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
message_protocol_result_t message_protocol_service_publish_event(uint8_t event)
{
    ++published;
    last_published = event;
    return publish_result;
}
void message_protocol_service_rx_isr(uint8_t byte, uint32_t received_ms)
{
    CHECK(received_ms == tick);
    ++protocol_rx_count;
    protocol_rx_byte = byte;
}
void message_protocol_service_rx_error_isr(void) {}
void message_protocol_service_clear_pending(void) { ++protocol_clear_count; }
static void reset_at(uint32_t now)
{
    host_gpio_a.input = host_gpio_b.input = host_gpio_c.input = UINT16_MAX;
    tick = now;
    published = 0U;
    last_published = MSG_NONE;
    publish_result = MESSAGE_PROTOCOL_OK;
    protocol_rx_count = 0U;
    protocol_rx_byte = 0U;
    protocol_clear_count = 0U;
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
    CHECK(published == 1U && last_published == MSG_STOP_REQUEST &&
        uart_service_get_tx_count() == 1U);
    CHECK(poll_after(1000U) == MSG_NONE && published == 1U);
    host_gpio_b.input |= GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_NONE);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_STOP_REQUEST);
    CHECK(published == 2U && last_published == MSG_STOP_REQUEST);
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
    CHECK(poll_after(1U) == MSG_STOP_REQUEST && published == 1U);

    reset_at(0U);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    button_init(); /* Already held at boot: require release and a new press. */
    CHECK(poll_after(1000U) == MSG_NONE && published == 0U);
    host_gpio_b.input |= GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_NONE);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_STOP_REQUEST && published == 1U);
}
static void wraparound_and_transport_failure(void)
{
    reset_at(UINT32_MAX - 10U);
    host_gpio_b.input &= (uint16_t)~GPIO_PIN_6;
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(29U) == MSG_NONE);
    publish_result = MESSAGE_PROTOCOL_IO_ERROR;
    CHECK(poll_after(1U) == MSG_STOP_REQUEST);
    CHECK(published == 1U && uart_service_get_tx_count() == 0U);
    CHECK(uart_service_get_status() == HAL_ERROR);
    CHECK(poll_after(1000U) == MSG_NONE && published == 1U);
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
    for (size_t i = 0U; i < 3U; ++i) {
        reset_at(100U);
        CHECK(press_and_release(ports[i], pins[i]) == events[i]);
        CHECK(published == 1U && last_published == (uint8_t)events[i]);
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
    CHECK(published == 0U);
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
    CHECK(published == 0U);
}

static void framed_uart_bytes_are_forwarded_and_can_be_cleared(void)
{
    reset_at(100U);
    CHECK(receive_target != NULL);

    *receive_target = (uint8_t)MSG_SPEED_UP_REQUEST;
    HAL_UART_RxCpltCallback(&uart1);
    CHECK(protocol_rx_count == 1U &&
        protocol_rx_byte == (uint8_t)MSG_SPEED_UP_REQUEST);
    uart_service_clear_pending();
    CHECK(protocol_clear_count == 1U);

    *receive_target = (uint8_t)MSG_STOP_REQUEST;
    HAL_UART_RxCpltCallback(&uart1);
    CHECK(protocol_rx_count == 2U &&
        protocol_rx_byte == (uint8_t)MSG_STOP_REQUEST);
}
static void framed_protocol_owns_transmit(void)
{
    reset_at(0U);
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_OK);
    CHECK(published == 1U);
    CHECK(uart_service_get_tx_count() == 1U);
    publish_result = MESSAGE_PROTOCOL_IO_ERROR;
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_ERROR);
    CHECK(published == 2U && uart_service_get_tx_count() == 1U);
    publish_result = MESSAGE_PROTOCOL_OK;
    CHECK(uart_service_send_message(MSG_STOP_REQUEST) == HAL_OK);
    CHECK(published == 3U && uart_service_get_tx_count() == 2U);
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
    framed_uart_bytes_are_forwarded_and_can_be_cleared();
    framed_protocol_owns_transmit();
    puts("PASS PB6 active-low: 29/30ms debounce, one STOP request, hold/release/repress");
    puts("PASS BTN1=UP, BTN2=DOWN, BTN3=STOP, BTN4 has no Mesh message");
    puts("PASS BTN1 debounced stable press/hold/release state and invalid button ID");
    puts("PASS BTN4 reset is one-shot, wins over a simultaneous message, and clears pending UART RX");
    puts("PASS bounce, boot-held, tick wrap, and transport failure");
    puts("HARDWARE_PIN_ROUTING_AND_MESH=NOT_TESTED");
    return 0;
}
