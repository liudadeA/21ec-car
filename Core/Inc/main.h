/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  * This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "ring_buffer.h"
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
#define RED_LED_PIN_Pin GPIO_PIN_3
#define RED_LED_PIN_GPIO_Port GPIOE
#define BIN1_Pin GPIO_PIN_3
#define BIN1_GPIO_Port GPIOA
#define AIN1_Pin GPIO_PIN_4
#define AIN1_GPIO_Port GPIOA
#define BIN2_Pin GPIO_PIN_5
#define BIN2_GPIO_Port GPIOA
#define AIN2_Pin GPIO_PIN_6
#define AIN2_GPIO_Port GPIOA
#define GREEN_LED_PIN_Pin GPIO_PIN_2
#define GREEN_LED_PIN_GPIO_Port GPIOB
#define Self_Led_Pin GPIO_PIN_4
#define Self_Led_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
// ================= 8-Channel Sensor State Macros =================
#define SENSOR_7  (1 << 7)  // Most right sensor
#define SENSOR_6  (1 << 6)
#define SENSOR_5  (1 << 5)
#define SENSOR_4  (1 << 4)  // Center-right
#define SENSOR_3  (1 << 3)  // Center-left
#define SENSOR_2  (1 << 2)
#define SENSOR_1  (1 << 1)
#define SENSOR_0  (1 << 0)  // Most left sensor

// ================= 8-Channel Key State Macros =================
// These macros define the line-following behavior based on which sensors are active.
#define STATE_STRAIGHT        (SENSOR_3 | SENSOR_4)
#define STATE_ADJUST_LEFT_1   (SENSOR_4)
#define STATE_ADJUST_LEFT_2   (SENSOR_2 | SENSOR_3)
#define STATE_ADJUST_RIGHT_1  (SENSOR_3)
#define STATE_ADJUST_RIGHT_2  (SENSOR_4 | SENSOR_5)
#define STATE_CURVE_LEFT      (SENSOR_0 | SENSOR_1)
#define STATE_CURVE_RIGHT     (SENSOR_6 | SENSOR_7)

// Special states
#define STATE_CROSSROAD       0xFF  // All sensors triggered
#define STATE_LOST_LINE       0x00  // No sensors triggered (Arrival or Line Loss)


extern volatile uint8_t In_Turn;

// ================= Function Declarations =================
// (Assuming these are defined elsewhere, e.g., in Control.h or another module)
void Motor_Init(void);
void Motor_PID_Init(void);
void Set_Motor_Target_Speed(float left_speed, float right_speed);
void Car_Stop(void);
void Pi_Start(void);
uint16_t BT_GetPacket(char* payload);
void BT_SendPacket(const char* payload, uint16_t len);
uint16_t GetPacket(UART_HandleTypeDef* huart, RingBuffer* ring_buffer, char* payload);
void Send_Packet(UART_HandleTypeDef* huart, const char* payload, uint16_t len);
void USART1_ProcessReceivedData(void);
void USART2_ProcessReceivedData(void);


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
