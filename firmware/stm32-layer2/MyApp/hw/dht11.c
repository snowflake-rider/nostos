/* Adapted from snowflake-rider/stm32-project commit 940ff240. */
#include "dht11.h"

#include <stddef.h>

#define DHT11_EDGE_TIMEOUT_US 120U

static GPIO_TypeDef *data_port;
static uint16_t data_pin;

static uint32_t cycles_per_microsecond(void)
{
    return SystemCoreClock / 1000000U;
}

static void delay_us(uint32_t microseconds)
{
    uint32_t ticks = cycles_per_microsecond() * microseconds;
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < ticks) { }
}

static void configure_output(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = data_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(data_port, &gpio);
}

static void configure_input(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = data_pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(data_port, &gpio);
}

static bool wait_while(GPIO_PinState state, uint32_t timeout_us)
{
    uint32_t timeout_ticks = cycles_per_microsecond() * timeout_us;
    uint32_t start = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(data_port, data_pin) == state)
    {
        if ((uint32_t)(DWT->CYCCNT - start) >= timeout_ticks) return false;
    }
    return true;
}

bool dht11_init(GPIO_TypeDef *port, uint16_t pin)
{
    if ((port == NULL) || (pin == 0U) || (cycles_per_microsecond() == 0U))
    {
        data_port = NULL;
        data_pin = 0U;
        return false;
    }
    data_port = port;
    data_pin = pin;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    configure_input();
    return true;
}

bool dht11_read(dht11_data_t *data)
{
    if ((data == NULL) || (data_port == NULL)) return false;

    uint8_t bytes[5] = {0};
    configure_output();
    HAL_GPIO_WritePin(data_port, data_pin, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(data_port, data_pin, GPIO_PIN_SET);
    delay_us(30U);
    configure_input();

    uint32_t previous_primask = __get_PRIMASK();
    __disable_irq();
    bool ok = wait_while(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US) &&
              wait_while(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US) &&
              wait_while(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US);

    for (uint8_t bit = 0U; ok && (bit < 40U); ++bit)
    {
        if (!wait_while(GPIO_PIN_RESET, DHT11_EDGE_TIMEOUT_US))
        {
            ok = false;
            break;
        }
        uint32_t high_start = DWT->CYCCNT;
        if (!wait_while(GPIO_PIN_SET, DHT11_EDGE_TIMEOUT_US))
        {
            ok = false;
            break;
        }
        uint32_t high_us =
            (uint32_t)(DWT->CYCCNT - high_start) / cycles_per_microsecond();
        bytes[bit / 8U] <<= 1U;
        if (high_us > 50U) bytes[bit / 8U] |= 1U;
    }
    if (previous_primask == 0U) __enable_irq();

    uint8_t checksum =
        (uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]);
    if (!ok || (checksum != bytes[4])) return false;

    data->humidity_x10 =
        (uint16_t)((uint16_t)bytes[0] * 10U + (uint16_t)bytes[1]);
    data->temperature_x10 =
        (int16_t)((uint16_t)(bytes[2] & 0x7FU) * 10U + bytes[3]);
    if ((bytes[2] & 0x80U) != 0U)
    {
        data->temperature_x10 = (int16_t)-data->temperature_x10;
    }
    return true;
}
