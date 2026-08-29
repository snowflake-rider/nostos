#include "button_output_test.h"

#include "audio_service.h"
#include "button.h"
#include "cheer_up_audio.h"
#include "main.h"
#include "message_service.h"
#include "speed_down_request_audio.h"
#include "speed_up_request_audio.h"
#include "stop_request_audio.h"
#include "vs1003b.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { if (!(expression)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef test_gpio_a;
GPIO_TypeDef test_gpio_b;
GPIO_TypeDef test_gpio_c;

static SPI_HandleTypeDef codec_spi = {2U};
static uint32_t tick_ms;
static bool dreq_ready;
static uint8_t first_sdi[32];
static size_t first_sdi_size;
static uint32_t sdi_bytes;
static uint32_t sdi_calls;

uint32_t HAL_GetTick(void)
{
    return tick_ms;
}

void HAL_Delay(uint32_t ms)
{
    tick_ms += ms;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    if ((port == VS_DREQ_GPIO_Port) && (pin == VS_DREQ_Pin))
    {
        return dreq_ready ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    return ((port->input & pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    if (state == GPIO_PIN_SET)
    {
        port->output |= pin;
    }
    else
    {
        port->output &= (uint16_t)~pin;
    }
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx,
                                         uint8_t *rx, uint16_t size,
                                         uint32_t timeout)
{
    CHECK(spi == &codec_spi);
    CHECK(tx != NULL && rx != NULL && timeout == 10U);
    memset(rx, 0, size);

    if ((VS_XCS_GPIO_Port->output & VS_XCS_Pin) == 0U)
    {
        CHECK(size == 4U && tx[0] == 0x03U);
        rx[2] = 0x08U;
        rx[3] = 0x00U;
        return HAL_OK;
    }

    CHECK((VS_XDCS_GPIO_Port->output & VS_XDCS_Pin) == 0U);
    CHECK(dreq_ready && size > 0U && size <= 32U);
    if (sdi_calls == 0U)
    {
        memcpy(first_sdi, tx, size);
        first_sdi_size = size;
    }
    ++sdi_calls;
    sdi_bytes += size;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *tx,
                                  uint16_t size, uint32_t timeout)
{
    CHECK(spi == &codec_spi && tx != NULL && size == 4U && timeout == 10U);
    return HAL_OK;
}

static void reset_chain(void)
{
    test_gpio_a = (GPIO_TypeDef){.input = UINT16_MAX};
    test_gpio_b = (GPIO_TypeDef){.input = UINT16_MAX};
    test_gpio_c = (GPIO_TypeDef){.input = UINT16_MAX};
    tick_ms = 0U;
    dreq_ready = true;
    first_sdi_size = 0U;
    sdi_bytes = 0U;
    sdi_calls = 0U;

    button_init();
    uint16_t mode = 0U;
    CHECK(vs1003b_init(&codec_spi, &mode) == VS1003B_STATUS_OK);
    CHECK(mode == 0x0800U);
    message_service_init(VS1003B_STATUS_OK);
    button_output_test_init();
}

static void process_after(uint32_t elapsed_ms)
{
    tick_ms += elapsed_ms;
    button_output_test_process();
}

static void press(GPIO_TypeDef *port, uint16_t pin)
{
    port->input &= (uint16_t)~pin;
    process_after(0U);
    CHECK(button_output_test_get_status()->press_count == 0U);
    process_after(29U);
    CHECK(button_output_test_get_status()->press_count == 0U);
    process_after(1U);
}

static void check_rgb(bool red, bool green, bool blue)
{
    CHECK(((RGB_R_GPIO_Port->output & RGB_R_Pin) != 0U) == red);
    CHECK(((RGB_G_GPIO_Port->output & RGB_G_Pin) != 0U) == green);
    CHECK(((RGB_B_GPIO_Port->output & RGB_B_Pin) != 0U) == blue);
}

static void drain_audio(uint32_t expected_size)
{
    for (uint32_t budget = 0U;
         audio_service_is_playing() && (budget < 100000U);
         ++budget)
    {
        button_output_test_process();
    }

    CHECK(!audio_service_is_playing());
    CHECK(button_output_test_get_status()->audio_status == VS1003B_STATUS_OK);
    CHECK(button_output_test_get_status()->audio_position == expected_size);
    CHECK(sdi_bytes == expected_size + 32U);
}

static void run_case(GPIO_TypeDef *port, uint16_t pin, message_type_t message,
                     bool red, bool green, bool blue,
                     const uint8_t *asset, uint32_t asset_size)
{
    reset_chain();
    press(port, pin);

    const button_output_test_status_t *status = button_output_test_get_status();
    CHECK(status->press_count == 1U && status->last_message == message);
    CHECK(status->rgb_active);
    check_rgb(red, green, blue);
    CHECK(status->audio_playing && status->audio_position == 32U);
    CHECK(first_sdi_size == 32U && memcmp(first_sdi, asset, 32U) == 0);

    drain_audio(asset_size);
    CHECK(button_output_test_get_status()->press_count == 1U);
    process_after(1999U);
    CHECK(button_output_test_get_status()->rgb_active);
    check_rgb(red, green, blue);
    process_after(1U);
    CHECK(!button_output_test_get_status()->rgb_active);
    check_rgb(false, false, false);
}

int main(void)
{
    run_case(GPIOB, GPIO_PIN_5, MSG_SPEED_DOWN_REQUEST,
             false, true, false,
             speed_down_request_audio_data, speed_down_request_audio_size);
    run_case(GPIOB, GPIO_PIN_10, MSG_SPEED_UP_REQUEST,
             true, false, false,
             speed_up_request_audio_data, speed_up_request_audio_size);
    run_case(GPIOA, GPIO_PIN_8, MSG_SAFETY_REMINDER,
             false, false, true,
             cheer_up_audio_data, cheer_up_audio_size);
    run_case(GPIOC, GPIO_PIN_7, MSG_STOP_REQUEST,
             true, true, true,
             stop_request_audio_data, stop_request_audio_size);

    /* 사용자가 지정한 별도 PB6 버튼도 같은 STOP 전체 경로를 검증합니다. */
    run_case(GPIOB, GPIO_PIN_6, MSG_STOP_REQUEST,
             true, true, true,
             stop_request_audio_data, stop_request_audio_size);

    puts("PASS button debounce -> semantic RGB -> real MP3 asset -> VS1003B SDI <=32B");
    puts("PASS PB6 -> STOP(0x13) -> WHITE -> stop_request_audio -> SPI data complete");
    puts("HAL=MOCK; UART_MESH_TX=NOT_PERFORMED; PHYSICAL_RGB_AND_SPEAKER_SOUND=NOT_TESTED; FLASH=NOT_PERFORMED");
    return 0;
}
