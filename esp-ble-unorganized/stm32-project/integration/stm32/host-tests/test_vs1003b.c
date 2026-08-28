#include "vs1003b.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
static SPI_HandleTypeDef spi = {2U};
static uint32_t tick, tick_step;
static HAL_StatusTypeDef spi_result;
static unsigned sdi_bytes, sdi_calls;
static uint16_t registers[16];
static bool drop_sdi_commands;
static uint8_t asset[2600];

uint32_t HAL_GetTick(void) { uint32_t now = tick; tick += tick_step; return now; }
void HAL_Delay(uint32_t delay) { tick += delay; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->input & pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState value)
{
    if (value == GPIO_PIN_SET) port->output |= pin;
    else port->output &= (uint16_t)~pin;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *handle, uint8_t *tx,
                                        uint8_t *rx, uint16_t size, uint32_t timeout)
{
    CHECK(handle == &spi && timeout == 10U && tx != NULL && rx != NULL);
    memset(rx, 0, size);
    if ((GPIOB->output & VS_XCS_Pin) == 0U)
    {
        CHECK(GPIOC->output & VS_XDCS_Pin);
        CHECK(size == 4U && tx[0] == 0x03U && tx[1] < 16U);
        rx[2] = (uint8_t)(registers[tx[1]] >> 8U);
        rx[3] = (uint8_t)registers[tx[1]];
    }
    else
    {
        CHECK((GPIOC->output & VS_XDCS_Pin) == 0U);
        CHECK(size > 0U && size <= 32U);
        ++sdi_calls;
        if (spi_result == HAL_OK) sdi_bytes += size;
        if (!drop_sdi_commands && spi_result == HAL_OK && size == 8U &&
            tx[0] == 0x53U && tx[1] == 0x70U && tx[2] == 0xEEU && tx[3] == 0U)
            registers[8] = registers[0];
    }
    return spi_result;
}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *handle, uint8_t *data,
                                 uint16_t size, uint32_t timeout)
{
    CHECK(handle == &spi && size == 4U && timeout == 10U);
    CHECK(data[0] == 0x02U && data[1] < 16U);
    CHECK((GPIOB->output & VS_XCS_Pin) == 0U && (GPIOC->output & VS_XDCS_Pin));
    if (spi_result == HAL_OK)
        registers[data[1]] = (uint16_t)((uint16_t)data[2] << 8U) | data[3];
    return spi_result;
}

static void init_at(uint32_t now)
{
    host_gpio_a = host_gpio_b = host_gpio_c = (GPIO_TypeDef){0};
    GPIOC->input = VS_DREQ_Pin;
    tick = now;
    tick_step = 0U;
    spi_result = HAL_OK;
    sdi_bytes = sdi_calls = 0U;
    memset(registers, 0, sizeof(registers));
    registers[0] = 0x0800U;
    drop_sdi_commands = false;
    uint16_t mode = 0U;
    CHECK(vs1003b_init(&spi, &mode) == VS1003B_STATUS_OK);
    CHECK(mode == 0x0800U);
}
static void stalled_dreq_reports_error(void)
{
    init_at(UINT32_MAX - 1000U); /* Also exercise HAL tick wrap. */
    CHECK(vs1003b_play_start(asset, sizeof(asset)) == VS1003B_STATUS_OK);
    for (unsigned i = 0; i < 79U; ++i)
        CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(vs1003b_play_position() == 2528U); /* Captured hardware failure. */
    GPIOC->input = 0U;
    unsigned calls = sdi_calls;
    tick += 1999U;
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(vs1003b_is_playing() && sdi_calls == calls);
    ++tick;
    CHECK(vs1003b_play_process() == VS1003B_STATUS_DREQ_TIMEOUT);
    CHECK(!vs1003b_is_playing() && sdi_calls == calls);
    CHECK(vs1003b_play_position() == 2528U);
}
static void temporary_backpressure_and_completion(void)
{
    init_at(0U);
    CHECK(vs1003b_play_start(asset, 33U) == VS1003B_STATUS_OK);
    CHECK(vs1003b_play_start(asset, 33U) == VS1003B_STATUS_BUSY);
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(vs1003b_play_position() == 32U);
    GPIOC->input = 0U;
    tick += 1900U;
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(sdi_bytes == 32U);
    GPIOC->input = VS_DREQ_Pin;
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(vs1003b_play_position() == 33U);
    GPIOC->input = 0U;
    tick += 1900U;
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(vs1003b_is_playing()); /* Deadline refreshes on progress. */
    GPIOC->input = VS_DREQ_Pin;
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    CHECK(!vs1003b_is_playing() && sdi_bytes == 65U);
}
static void reset_cancels_old_stream(void)
{
    init_at(0U);
    CHECK(vs1003b_play_start(asset, sizeof(asset)) == VS1003B_STATUS_OK);
    CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
    init_at(0U);
    CHECK(!vs1003b_is_playing() && vs1003b_play_position() == 0U);
    CHECK(vs1003b_play_start(asset, 1U) == VS1003B_STATUS_OK);
    GPIOC->input = 0U;
    tick_step = 1U; /* Let the bounded hardware-reset wait expire. */
    uint16_t mode;
    CHECK(vs1003b_init(&spi, &mode) == VS1003B_STATUS_DREQ_TIMEOUT);
    CHECK(!vs1003b_is_playing());
}
static void transfer_errors_release_both_selects(void)
{
    for (unsigned finishing = 0U; finishing < 2U; ++finishing)
    {
        init_at(0U);
        CHECK(vs1003b_play_start(asset, 1U) == VS1003B_STATUS_OK);
        if (finishing) CHECK(vs1003b_play_process() == VS1003B_STATUS_OK);
        spi_result = HAL_ERROR;
        CHECK(vs1003b_play_process() == VS1003B_STATUS_SPI_ERROR);
        CHECK(!vs1003b_is_playing());
        CHECK((GPIOB->output & VS_XCS_Pin) && (GPIOC->output & VS_XDCS_Pin));
    }
}
static void sdi_test_requires_echo_not_just_spi_success(void)
{
    uint16_t echo = 0U;
    init_at(0U);
    CHECK(vs1003b_sdi_test(&echo) == VS1003B_STATUS_OK);
    CHECK(echo == 0x0820U && sdi_bytes == 8U);
    init_at(0U);
    drop_sdi_commands = true;
    CHECK(vs1003b_sdi_test(&echo) == VS1003B_STATUS_REGISTER_MISMATCH);
    CHECK(echo == 0U && sdi_bytes == 8U);
}
int main(void)
{
    stalled_dreq_reports_error();
    temporary_backpressure_and_completion();
    reset_cancels_old_stream();
    transfer_errors_release_both_selects();
    sdi_test_requires_echo_not_just_spi_success();
    puts("PASS real VS1003B driver: 2528-byte stall, timeout/wrap, backpressure, reset, SPI errors");
    puts("PHYSICAL_AUDIO=NOT_TESTED");
    return 0;
}
