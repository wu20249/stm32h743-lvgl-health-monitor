#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32h7xx_hal.h"

/*
 * 蜂鸣器使用 PA1
 * 高电平触发
 */
#define BUZZER_GPIO_PORT      GPIOA
#define BUZZER_GPIO_PIN       GPIO_PIN_1
#define BUZZER_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE()

#define BUZZER_ON()   HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_SET)
#define BUZZER_OFF()  HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, GPIO_PIN_RESET)
#define BUZZER_TOGGLE() HAL_GPIO_TogglePin(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN)

void buzzer_init(void);

#endif