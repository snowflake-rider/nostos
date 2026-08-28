/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define HCSR04_TRIG_Pin GPIO_PIN_0
#define HCSR04_TRIG_GPIO_Port GPIOC
#define RGB_B_Pin GPIO_PIN_1
#define RGB_B_GPIO_Port GPIOC
#define HCSR04_ECHO_Pin GPIO_PIN_0
#define HCSR04_ECHO_GPIO_Port GPIOA
#define RGB_R_Pin GPIO_PIN_4
#define RGB_R_GPIO_Port GPIOA
#define VS_DREQ_Pin GPIO_PIN_4
#define VS_DREQ_GPIO_Port GPIOC
#define VS_XDCS_Pin GPIO_PIN_5
#define VS_XDCS_GPIO_Port GPIOC
#define RGB_G_Pin GPIO_PIN_0
#define RGB_G_GPIO_Port GPIOB
#define VS_RST_Pin GPIO_PIN_1
#define VS_RST_GPIO_Port GPIOB
#define BTN2_SPEED_DOWN_Pin GPIO_PIN_10
#define BTN2_SPEED_DOWN_GPIO_Port GPIOB
#define VS_XCS_Pin GPIO_PIN_12
#define VS_XCS_GPIO_Port GPIOB
#define BTN4_STOP_Pin GPIO_PIN_7
#define BTN4_STOP_GPIO_Port GPIOC
#define BTN3_SAFETY_Pin GPIO_PIN_8
#define BTN3_SAFETY_GPIO_Port GPIOA
#define BUZZER_Pin GPIO_PIN_4
#define BUZZER_GPIO_Port GPIOB
#define BTN1_SPEED_UP_Pin GPIO_PIN_5
#define BTN1_SPEED_UP_GPIO_Port GPIOB
#define TEST_BUTTON_Pin GPIO_PIN_6
#define TEST_BUTTON_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
