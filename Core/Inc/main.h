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

#include "stm32f4xx_nucleo.h"

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUTTON_MODE_Pin GPIO_PIN_0
#define BUTTON_MODE_GPIO_Port GPIOC
#define BUTTON_MODE_EXTI_IRQn EXTI0_IRQn
#define BUTTON_TEMP_UP_Pin GPIO_PIN_1
#define BUTTON_TEMP_UP_GPIO_Port GPIOC
#define BUTTON_TEMP_UP_EXTI_IRQn EXTI1_IRQn
#define BUTTON_TEMP_DOWN_Pin GPIO_PIN_2
#define BUTTON_TEMP_DOWN_GPIO_Port GPIOC
#define BUTTON_TEMP_DOWN_EXTI_IRQn EXTI2_IRQn
#define BUTTON_RESET_Pin GPIO_PIN_3
#define BUTTON_RESET_GPIO_Port GPIOC
#define BUTTON_RESET_EXTI_IRQn EXTI3_IRQn
#define LM35_Pin GPIO_PIN_1
#define LM35_GPIO_Port GPIOA
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define FAN_PWM_Pin GPIO_PIN_6
#define FAN_PWM_GPIO_Port GPIOA
#define BUTTON_MUTE_Pin GPIO_PIN_4
#define BUTTON_MUTE_GPIO_Port GPIOC
#define BUTTON_MUTE_EXTI_IRQn EXTI4_IRQn
#define LCD_D6_Pin GPIO_PIN_10
#define LCD_D6_GPIO_Port GPIOB
#define HEATER_RELAY_Pin GPIO_PIN_6
#define HEATER_RELAY_GPIO_Port GPIOC
#define LCD_EN_Pin GPIO_PIN_7
#define LCD_EN_GPIO_Port GPIOC
#define BUZZER_Pin GPIO_PIN_8
#define BUZZER_GPIO_Port GPIOC
#define DHT11_Pin GPIO_PIN_9
#define DHT11_GPIO_Port GPIOC
#define LCD_D7_Pin GPIO_PIN_8
#define LCD_D7_GPIO_Port GPIOA
#define LCD_RS_Pin GPIO_PIN_9
#define LCD_RS_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define LCD_D5_Pin GPIO_PIN_4
#define LCD_D5_GPIO_Port GPIOB
#define EEPROM_SCL_Pin GPIO_PIN_8
#define EEPROM_SCL_GPIO_Port GPIOB
#define EEPROM_SDA_Pin GPIO_PIN_9
#define EEPROM_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
