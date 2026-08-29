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
        CHECK(!button_take_calibration_request());
    }
}

static void btn4_alone_has_no_output(void)
{
    reset_at(100U);
    CHECK(press_and_release(GPIOC, GPIO_PIN_7) == MSG_NONE);
    CHECK(sent == 0U);
    CHECK(!button_take_calibration_request());
}

static void calibration_sequence_is_local_and_one_shot(void)
{
    GPIO_TypeDef *ports[] = {GPIOB, GPIOB, GPIOA, GPIOC};
    const uint16_t pins[] = {
        GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_8, GPIO_PIN_7
    };
    const message_type_t events[] = {
        MSG_SPEED_UP_REQUEST, MSG_SPEED_DOWN_REQUEST, MSG_STOP_REQUEST, MSG_NONE
    };

    reset_at(100U);
    for (size_t i = 0U; i < 4U; ++i) {
        CHECK(press_and_release(ports[i], pins[i]) == events[i]);
        CHECK(button_take_calibration_request() == (i == 3U));
    }
    CHECK(!button_take_calibration_request());
    CHECK(sent == 3U && last_byte == 0x13U);

    /* 디바운싱 시간 계산은 HAL tick wraparound에서도 유지됩니다. */
    reset_at(UINT32_MAX - 100U);
    for (size_t i = 0U; i < 4U; ++i) {
        CHECK(press_and_release(ports[i], pins[i]) == events[i]);
    }
    CHECK(button_take_calibration_request());
    CHECK(!button_take_calibration_request() && sent == 3U);
}

static void wrong_or_slow_sequence_is_rejected(void)
{
    reset_at(100U);
    CHECK(press_and_release(GPIOB, GPIO_PIN_5) == MSG_SPEED_UP_REQUEST);
    CHECK(press_and_release(GPIOA, GPIO_PIN_8) == MSG_STOP_REQUEST);
    CHECK(press_and_release(GPIOB, GPIO_PIN_10) == MSG_SPEED_DOWN_REQUEST);
    CHECK(press_and_release(GPIOC, GPIO_PIN_7) == MSG_NONE);
    CHECK(!button_take_calibration_request());

    reset_at(100U);
    CHECK(press_and_release(GPIOB, GPIO_PIN_5) == MSG_SPEED_UP_REQUEST);
    CHECK(poll_after(5000U) == MSG_NONE);
    CHECK(press_and_release(GPIOB, GPIO_PIN_10) == MSG_SPEED_DOWN_REQUEST);
    CHECK(press_and_release(GPIOA, GPIO_PIN_8) == MSG_STOP_REQUEST);
    CHECK(press_and_release(GPIOC, GPIO_PIN_7) == MSG_NONE);
    CHECK(!button_take_calibration_request());
}

static void simultaneous_buttons_are_not_a_sequence(void)
{
    GPIO_TypeDef *ports[] = {GPIOB, GPIOB, GPIOA, GPIOC};
    const uint16_t pins[] = {
        GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_8, GPIO_PIN_7
    };

    reset_at(100U);
    for (size_t index = 0U; index < 4U; ++index) {
        ports[index]->input &= (uint16_t)~pins[index];
    }
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_SPEED_UP_REQUEST);
    CHECK(!button_take_calibration_request());

    for (size_t index = 0U; index < 4U; ++index) {
        ports[index]->input |= pins[index];
    }
    CHECK(poll_after(0U) == MSG_NONE);
    CHECK(poll_after(30U) == MSG_NONE);

    CHECK(press_and_release(GPIOB, GPIO_PIN_5) == MSG_SPEED_UP_REQUEST);
    CHECK(press_and_release(GPIOB, GPIO_PIN_10) == MSG_SPEED_DOWN_REQUEST);
    CHECK(press_and_release(GPIOA, GPIO_PIN_8) == MSG_STOP_REQUEST);
    CHECK(press_and_release(GPIOC, GPIO_PIN_7) == MSG_NONE);
    CHECK(button_take_calibration_request());
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
    btn4_alone_has_no_output();
    calibration_sequence_is_local_and_one_shot();
    wrong_or_slow_sequence_is_rejected();
    simultaneous_buttons_are_not_a_sequence();
    usb_trace_does_not_change_transport_result();
    puts("PASS PB6 active-low: 29/30ms debounce, one 0x13 byte via selected UART, hold/release/repress");
    puts("PASS BTN1=UP/0x11, BTN2=DOWN/0x10, BTN3=STOP/0x13, BTN4=no message");
    puts("PASS BTN1->BTN2->BTN3->BTN4=one local calibration request within 5 seconds");
    puts("PASS wrong/slow/simultaneous sequence rejection, bounce, boot-held, tick wrap, transport failure");
    puts("HARDWARE_PIN_ROUTING_AND_MESH=NOT_TESTED");
    return 0;
}
