#include "buzzer.h"

/**
 * @brief 初始化蜂鸣器 GPIO
 *        PA1，推挽输出，默认低电平关闭
 */
void buzzer_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    BUZZER_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = BUZZER_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(BUZZER_GPIO_PORT, &gpio_init_struct);

    /*
     * 默认关闭蜂鸣器
     */
    BUZZER_OFF();
}