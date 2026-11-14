/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */
#include "app_state.h"

/*
 * 四路 LED（高电平点亮，与 MX_GPIO_Init 里初值 SET 一致）：
 *   LED1 (PB15) = 自动模式
 *   LED2 (PB14) = 手动模式
 *   LED3 (PB13) = 环境正常
 *   LED4 (PB12) = 报警
 * 模式灯与状态灯各亮一只，四种组合对应四个状态。
 */
#define LED_ON   GPIO_PIN_SET
#define LED_OFF  GPIO_PIN_RESET

static void LED_Write(GPIO_TypeDef *port, uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(port, pin, (on != 0u) ? LED_ON : LED_OFF);
}
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RELAY2_Pin|RELAY1_Pin|RELAY3_Pin|RELAY4_Pin
                          |DHT11_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED4_Pin|LED3_Pin|LED2_Pin|LED1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : RELAY2_Pin RELAY1_Pin RELAY3_Pin RELAY4_Pin
                           DHT11_Pin */
  GPIO_InitStruct.Pin = RELAY2_Pin|RELAY1_Pin|RELAY3_Pin|RELAY4_Pin
                          |DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : BEEP_Pin LED4_Pin LED3_Pin LED2_Pin
                           LED1_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin|LED4_Pin|LED3_Pin|LED2_Pin
                          |LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY1_Pin KEY2_Pin KEY3_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin|KEY2_Pin|KEY3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY4_Pin KEY5_Pin KEY6_Pin */
  GPIO_InitStruct.Pin = KEY4_Pin|KEY5_Pin|KEY6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

/* 四灯：自动/手动 + 正常/报警，每 100ms 由 ControlTask 刷一次 */
void LED_Scan(void) {
    LED_Write(LED1_GPIO_Port, LED1_Pin, (g_app.ctrl_mode == 0u) ? 1u : 0u);
    LED_Write(LED2_GPIO_Port, LED2_Pin, (g_app.ctrl_mode != 0u) ? 1u : 0u);
    LED_Write(LED3_GPIO_Port, LED3_Pin, (g_app.alarm != 0u) ? 1u : 0u);
    LED_Write(LED4_GPIO_Port, LED4_Pin, (g_app.alarm == 0u) ? 1u : 0u);
}

/*
 * 逻辑开关 -> 继电器（高电平吸合）。
 * 唯一映射点：RELAY1=风扇 RELAY2=加湿器 RELAY3=补光灯 RELAY4=水泵。
 */
void GPIO_RelayApply(void) {
    HAL_GPIO_WritePin(RELAY1_GPIO_Port, RELAY1_Pin,
                      (g_app.env_sw.fan_on  != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RELAY2_GPIO_Port, RELAY2_Pin,
                      (g_app.env_sw.mist_on != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RELAY3_GPIO_Port, RELAY3_Pin,
                      (g_app.env_sw.lamp_on != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RELAY4_GPIO_Port, RELAY4_Pin,
                      (g_app.env_sw.pump_on != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* USER CODE END 2 */
