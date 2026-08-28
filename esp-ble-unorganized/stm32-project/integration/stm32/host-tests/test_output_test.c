#include "output_test.h"
#include "audio_service.h"
#include "button.h"
#include "buzzer.h"
#include "main.h"
#include "message_router.h"
#include "message_service.h"
#include "uart_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static UART_HandleTypeDef usb = {2U};
static SPI_HandleTypeDef spi = {2U};
static uint8_t command;
static uint16_t registers[16];
static unsigned codec_resets, sine_starts, sine_stops, sdi_tests;
static vs1003b_status_t init_result, sine_result, stop_result;
static uint32_t tick;
static message_service_status_t service;
static bool playing;
static uint32_t position;
static message_type_t last_sent, remote;
static unsigned sent, starts;
static unsigned wire_sent;
static HAL_StatusTypeDef wire_result;
static char logs[16384];
static size_t log_size;

uint32_t HAL_GetTick(void) { return tick; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->input & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) port->output |= pin;
    else port->output &= (uint16_t)~pin;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, const uint8_t *data,
                                  uint16_t size, uint32_t timeout)
{
    CHECK(uart == &usb && timeout == 50U && data != NULL);
    CHECK(log_size + size < sizeof(logs));
    memcpy(logs + log_size, data, size);
    log_size += size;
    logs[log_size] = '\0';
    return HAL_OK;
}
bool audio_service_is_playing(void) { return playing; }
uint32_t audio_service_position(void) { return position; }
bool vs1003b_is_ready(void) { return true; }
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *uart, uint8_t *data,
                                  uint16_t size, uint32_t timeout)
{
    CHECK(uart == &usb && size == 1U && timeout == 0U);
    if (!command) return HAL_TIMEOUT;
    *data = command;
    command = 0U;
    return HAL_OK;
}
vs1003b_status_t vs1003b_init(SPI_HandleTypeDef *handle, uint16_t *mode)
{
    CHECK(handle == &spi);
    ++codec_resets;
    playing = false;
    position = 0U;
    *mode = 0x0800U;
    return init_result;
}
vs1003b_status_t vs1003b_read_register(uint8_t address, uint16_t *value)
{
    CHECK(address < 16U);
    *value = registers[address];
    return VS1003B_STATUS_OK;
}
vs1003b_status_t vs1003b_write_register(uint8_t address, uint16_t value)
{
    CHECK(address < 16U);
    registers[address] = value;
    return VS1003B_STATUS_OK;
}
vs1003b_status_t vs1003b_sine_test_start(void) { ++sine_starts; return sine_result; }
vs1003b_status_t vs1003b_sine_test_stop(void) { ++sine_stops; return stop_result; }
vs1003b_status_t vs1003b_sdi_test(uint16_t *echo)
{
    ++sdi_tests;
    *echo = 0x0820U;
    return VS1003B_STATUS_OK;
}
void message_service_init(vs1003b_status_t status) { service.audio_status = status; }
const message_service_status_t *message_service_get_status(void) { return &service; }
void message_service_process(void) { buzzer_process(); }
HAL_StatusTypeDef message_router_publish_local(message_type_t message)
{
    ++sent;
    last_sent = message;
    if (!playing && service.audio_status == VS1003B_STATUS_OK)
    {
        playing = true;
        position = 0U;
        ++starts;
    }
    return HAL_OK;
}
bool uart_service_get_message(message_type_t *message)
{
    if (remote == MSG_NONE) return false;
    *message = remote;
    remote = MSG_NONE;
    return true;
}
HAL_StatusTypeDef uart_service_send_message(message_type_t message)
{
    ++wire_sent;
    last_sent = message;
    return wire_result;
}

static void step(uint32_t ms) { tick += ms; output_test_process(); }
static void reset_at(uint32_t now, vs1003b_status_t status)
{
    host_gpio_a = host_gpio_b = host_gpio_c = (GPIO_TypeDef){UINT16_MAX, 0U};
    tick = now;
    service = (message_service_status_t){.audio_status = status};
    playing = false;
    position = 0U;
    sent = starts = 0U;
    wire_sent = 0U;
    wire_result = HAL_OK;
    last_sent = remote = MSG_NONE;
    log_size = 0U;
    logs[0] = '\0';
    command = 0U;
    codec_resets = sine_starts = sine_stops = sdi_tests = 0U;
    init_result = sine_result = stop_result = VS1003B_STATUS_OK;
    memset(registers, 0, sizeof(registers));
    registers[0] = 0x0800U;
    button_init();
    output_test_init(&usb, &spi);
}
static void press(GPIO_TypeDef *port, uint16_t pin)
{
    port->input &= (uint16_t)~pin;
    step(0U);
    step(30U);
}
static void four_buttons_and_output_timing(void)
{
    GPIO_TypeDef *ports[] = {GPIOB, GPIOB, GPIOA, GPIOC};
    const uint16_t pins[] = {GPIO_PIN_5, GPIO_PIN_10, GPIO_PIN_8, GPIO_PIN_7};
    const message_type_t ids[] = {MSG_SPEED_UP_REQUEST, MSG_SPEED_DOWN_REQUEST,
                                 MSG_SAFETY_REMINDER, MSG_STOP_REQUEST};
    for (size_t i = 0U; i < 4U; ++i)
    {
        reset_at(UINT32_MAX - 200U, VS1003B_STATUS_OK);
        press(ports[i], pins[i]);
        CHECK(sent == 1U && last_sent == ids[i] && starts == 1U);
        CHECK(((GPIOA->output & GPIO_PIN_4) != 0U) == (i == 0U || i == 3U));
        CHECK(((GPIOB->output & GPIO_PIN_0) != 0U) == (i == 1U || i == 3U));
        CHECK(((GPIOC->output & GPIO_PIN_1) != 0U) == (i == 2U || i == 3U));
        CHECK((GPIOB->output & GPIO_PIN_4) != 0U);
        step(99U); CHECK(buzzer_is_active());
        step(1U); CHECK(!buzzer_is_active());
        step(100U); CHECK(buzzer_is_active());
        step(100U); CHECK(!buzzer_is_active());
        CHECK((GPIOB->output & GPIO_PIN_4) == 0U);
        step(1699U); CHECK(strstr(logs, "RGB_OFF") == NULL);
        playing = false;
        position = 19917U;
        step(1U);
        CHECK((GPIOA->output | GPIOB->output | GPIOC->output) == 0U);
        CHECK(sent == 1U); /* Holding a button must not repeat the request. */
        CHECK(strstr(logs, "RGB_OFF") != NULL);
        CHECK(strstr(logs, "AUDIO_DATA_DONE") != NULL);
    }
}
static void audio_error_and_busy_are_visible(void)
{
    reset_at(0U, VS1003B_STATUS_MODE_MISMATCH);
    press(GPIOB, GPIO_PIN_5);
    CHECK(sent == 1U && starts == 0U);
    CHECK(GPIOA->output & GPIO_PIN_4);
    CHECK(buzzer_is_active());
    CHECK(strstr(logs, "audio=BLOCKED status=MODE_MISMATCH") != NULL);

    reset_at(0U, VS1003B_STATUS_OK);
    press(GPIOB, GPIO_PIN_5);
    press(GPIOB, GPIO_PIN_10);
    CHECK(sent == 2U && starts == 1U);
    CHECK(strstr(logs, "audio=BUSY_SKIPPED") != NULL);
    step(2000U);
    CHECK(strstr(logs, "AUDIO_STALLED id=0x11") != NULL);
    remote = MSG_SOS;
    step(0U);
    CHECK(strstr(logs, "REMOTE_IGNORED id=0x31") != NULL);
    CHECK(sent == 2U && starts == 1U);
}
static void diagnostics_are_bounded_and_do_not_publish(void)
{
    reset_at(UINT32_MAX - 500U, VS1003B_STATUS_OK);
    command = 's'; step(0U);
    CHECK(sine_starts == 1U && codec_resets == 1U);
    press(GPIOB, GPIO_PIN_5);
    CHECK(sent == 0U && starts == 0U);
    step(969U); CHECK(sine_stops == 0U);
    stop_result = VS1003B_STATUS_DREQ_TIMEOUT;
    step(1U); CHECK(sine_stops == 1U && codec_resets == 2U);
    CHECK(strstr(logs, "SINE_STOP status=DREQ_TIMEOUT") != NULL);
    command = 't'; step(0U);
    CHECK(sdi_tests == 1U && codec_resets == 4U && sent == 0U);
    CHECK(strstr(logs, "SDI_TEST status=OK expected=0820 echo=0820") != NULL);

    init_result = VS1003B_STATUS_MODE_MISMATCH;
    command = 's'; step(0U);
    CHECK(sine_starts == 1U); /* No test sound after failed initialization. */
    CHECK(service.audio_status == VS1003B_STATUS_MODE_MISMATCH);
    init_result = VS1003B_STATUS_OK;
    command = 'r'; step(0U);
    CHECK(service.audio_status == VS1003B_STATUS_OK && !playing);
    CHECK(registers[0x03] == 0x9800U && registers[0x0B] == 0x5050U);
    press(GPIOB, GPIO_PIN_10);
    CHECK(sent == 1U && starts == 1U && last_sent == MSG_SPEED_DOWN_REQUEST);
}
static void message_test_uses_uart_without_local_outputs(void)
{
    const uint8_t ids[] = {0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x30, 0x31};
    reset_at(UINT32_MAX - 500U, VS1003B_STATUS_OK);
    command = '?'; step(0U);
    CHECK(strstr(logs, "MESSAGE_TEST_READY protocol=1") != NULL);
    command = 0x13; step(0U);
    CHECK(wire_sent == 0U); /* Unarmed input cannot transmit. */
    for (size_t i = 0; i < sizeof(ids); ++i)
    {
        command = 'm'; step(0U);
        CHECK(strstr(logs, "MESSAGE_TEST_ARMED") != NULL);
        command = ids[i]; step(0U);
        CHECK(wire_sent == i + 1U && last_sent == ids[i]);
        step(1U);
        CHECK(wire_sent == i + 1U); /* No periodic send or retry. */
    }
    CHECK(sent == 0U && starts == 0U && !playing && !buzzer_is_active());
    CHECK((GPIOA->output | GPIOB->output | GPIOC->output) == 0U);
    CHECK(strstr(logs, "MESSAGE_TEST_TX id=0x31 uart=OK seq=8") != NULL);
    command = 'm'; step(0U);
    command = 's'; step(0U); /* Malformed ID must not become a codec command. */
    CHECK(wire_sent == 8U && sine_starts == 0U && codec_resets == 0U);
    command = 'm'; step(0U);
    step(1000U); /* Includes uint32 tick wrap. */
    command = 0x13; step(0U);
    CHECK(wire_sent == 8U);
    wire_result = HAL_ERROR;
    command = 'm'; step(0U);
    command = 0x13; step(0U);
    CHECK(strstr(logs, "MESSAGE_TEST_TX id=0x13 uart=ERROR seq=9") != NULL);
    step(1000U);
    CHECK(wire_sent == 9U);
}
int main(void)
{
    four_buttons_and_output_timing();
    audio_error_and_busy_are_visible();
    diagnostics_are_bounded_and_do_not_publish();
    message_test_uses_uart_without_local_outputs();
    puts("PASS real button/RGB/buzzer drivers: mappings, debounce, finite outputs, tick wrap");
    puts("PASS audio failure/busy/stall reporting, remote output isolation (audio/router mocked)");
    puts("PHYSICAL_LIGHT_SOUND_AND_VS1003B=NOT_TESTED");
    return 0;
}
