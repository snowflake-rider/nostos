# Day 33 — 김현수 원문

출처: https://app.notion.com/3952ae7008718257bffd818ba48c8329

김현수 구역만 추출. ST 저작권 고지와 원문을 보존하며, 이 파일은 빌드하지 않습니다.

````c
/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

// ⭐ CHANGED: Make printf() send text through USART2.
//
// printf() normally writes to a computer terminal. An STM32 has no terminal,
// so we provide a small "bridge" function that sends each character to UART2.
// __GNUC__ is defined when the project is compiled with the GNU ARM compiler.
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE {
  // Send one character through USART2.
  // This is a blocking call: the CPU waits here until transmission finishes
  // or the 0xFFFF timeout expires.
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  // Return the transmitted character so printf() knows it was handled.
  return ch;
}

uint8_t rx_data;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define RX_BUF_SIZE 64

#define TIME_OUT 100

uint8_t usart1_rx_buf[RX_BUF_SIZE];
uint8_t usart2_rx_buf[RX_BUF_SIZE];

//
// STM32 flow:
// pin edge -> EXTI interrupt -> IRQ handler -> HAL -> this callback
//
// GPIO_Pin tells us which pin caused the callback. It is a pin bit mask such as
// GPIO_PIN_13 or GPIO_PIN_0, not a normal sequential pin number.
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_13) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
  } else if (GPIO_Pin == GPIO_PIN_0) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
  }
}

// HAL calls this function when Receive-to-Idle DMA has data ready.
// huart tells us which USART received the data.
// Size tells us how many bytes in that USART's buffer are valid.
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart->Instance == USART1) {
    // Data came from the other STM32 through USART1.
    // Send exactly those bytes to the Mac through USART2.
    HAL_UART_Transmit(&huart2, usart1_rx_buf, Size, TIME_OUT);

    // DMA is in Normal mode, so start USART1 reception again.
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, usart1_rx_buf, RX_BUF_SIZE);
  } else if (huart->Instance == USART2) {
    // Data came from the Mac through USART2.
    // Send exactly those bytes to the other STM32 through USART1.
    HAL_UART_Transmit(&huart1, usart2_rx_buf, Size, TIME_OUT);

    // DMA is in Normal mode, so start USART2 reception again.
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, usart2_rx_buf, RX_BUF_SIZE);
  }
}

// HAL calls this function if a USART receive error occurs.
// Stop the failed DMA reception, then start that USART again.
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    // Recover the connection from the other STM32.
    HAL_UART_DMAStop(&huart1);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, usart1_rx_buf, RX_BUF_SIZE);
  } else if (huart->Instance == USART2) {
    // Recover the connection from the Mac.
    HAL_UART_DMAStop(&huart2);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, usart2_rx_buf, RX_BUF_SIZE);
  }
}


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  // Start receiving bytes from the other STM32 through USART1.
  // DMA stores them in usart1_rx_buf until the line is idle or the buffer is
  // full.
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, usart1_rx_buf, RX_BUF_SIZE);

  // Start receiving bytes from the Mac through USART2.
  // DMA stores them in usart2_rx_buf until the line is idle or the buffer is
  // full.
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, usart2_rx_buf, RX_BUF_SIZE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state
   */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
     file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */


````
