#ifndef HOST_STM32F4XX_HAL_H
#define HOST_STM32F4XX_HAL_H
#include <stddef.h>
#include <stdint.h>

typedef struct { uint16_t input; uint16_t output; } GPIO_TypeDef;
extern GPIO_TypeDef host_gpio_a, host_gpio_b, host_gpio_c;
#define GPIOA (&host_gpio_a)
#define GPIOB (&host_gpio_b)
#define GPIOC (&host_gpio_c)
#define GPIO_PIN_0  ((uint16_t)0x0001)
#define GPIO_PIN_1  ((uint16_t)0x0002)
#define GPIO_PIN_4  ((uint16_t)0x0010)
#define GPIO_PIN_5  ((uint16_t)0x0020)
#define GPIO_PIN_6  ((uint16_t)0x0040)
#define GPIO_PIN_7  ((uint16_t)0x0080)
#define GPIO_PIN_8  ((uint16_t)0x0100)
#define GPIO_PIN_10 ((uint16_t)0x0400)
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { unsigned instance; } UART_HandleTypeDef;
typedef struct { unsigned instance; } I2C_HandleTypeDef;
typedef struct { unsigned instance; } SPI_HandleTypeDef;

uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, const uint8_t *data,
                                 uint16_t size, uint32_t timeout);
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#endif
