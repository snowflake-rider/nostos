#ifndef BUTTON_OUTPUT_TEST_HAL_H
#define BUTTON_OUTPUT_TEST_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint16_t input;
    uint16_t output;
} GPIO_TypeDef;

extern GPIO_TypeDef test_gpio_a;
extern GPIO_TypeDef test_gpio_b;
extern GPIO_TypeDef test_gpio_c;

#define GPIOA (&test_gpio_a)
#define GPIOB (&test_gpio_b)
#define GPIOC (&test_gpio_c)

#define GPIO_PIN_0  ((uint16_t)0x0001U)
#define GPIO_PIN_1  ((uint16_t)0x0002U)
#define GPIO_PIN_4  ((uint16_t)0x0010U)
#define GPIO_PIN_5  ((uint16_t)0x0020U)
#define GPIO_PIN_6  ((uint16_t)0x0040U)
#define GPIO_PIN_7  ((uint16_t)0x0080U)
#define GPIO_PIN_8  ((uint16_t)0x0100U)
#define GPIO_PIN_10 ((uint16_t)0x0400U)
#define GPIO_PIN_12 ((uint16_t)0x1000U)

typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { unsigned instance; } SPI_HandleTypeDef;
typedef struct { unsigned instance; } UART_HandleTypeDef;

uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx,
                                         uint8_t *rx, uint16_t size,
                                         uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *tx,
                                  uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data,
                                     uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart,
                                   const uint8_t *data, uint16_t size,
                                   uint32_t timeout);

#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)

#endif /* BUTTON_OUTPUT_TEST_HAL_H */
