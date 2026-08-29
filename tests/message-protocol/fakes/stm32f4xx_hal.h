#ifndef NOSTOS_TEST_HAL_H
#define NOSTOS_TEST_HAL_H
#include <stddef.h>
#include <stdint.h>
typedef struct { uint16_t output; } GPIO_TypeDef;
extern GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_c;
#define GPIOA (&test_gpio_a)
#define GPIOB (&test_gpio_b)
#define GPIOC (&test_gpio_c)
#define GPIO_PIN_0 ((uint16_t)1)
#define GPIO_PIN_1 ((uint16_t)2)
#define GPIO_PIN_2 ((uint16_t)4)
#define GPIO_PIN_3 ((uint16_t)8)
#define GPIO_PIN_4 ((uint16_t)16)
#define GPIO_PIN_5 ((uint16_t)32)
#define GPIO_PIN_6 ((uint16_t)64)
#define GPIO_PIN_7 ((uint16_t)128)
#define GPIO_PIN_8 ((uint16_t)256)
#define GPIO_PIN_9 ((uint16_t)512)
#define GPIO_PIN_10 ((uint16_t)1024)
#define GPIO_PIN_11 ((uint16_t)2048)
#define GPIO_PIN_12 ((uint16_t)4096)
#define GPIO_PIN_13 ((uint16_t)8192)
#define GPIO_PIN_14 ((uint16_t)16384)
#define GPIO_PIN_15 ((uint16_t)32768)
typedef struct { unsigned instance; } SPI_HandleTypeDef;
typedef struct { unsigned instance; } UART_HandleTypeDef;
typedef struct { unsigned instance; } I2C_HandleTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
void HAL_GPIO_WritePin(GPIO_TypeDef *port,uint16_t pin,GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port,uint16_t pin);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,uint8_t *tx,uint8_t *rx,uint16_t n,uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi,uint8_t *tx,uint16_t n,uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart,uint8_t *tx,uint16_t n,uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart,uint8_t *rx,uint16_t n);
static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __disable_irq(void) {}
static inline void __enable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) { (void)mask; }
#endif
