/**
 ****************************************************************************************************
 * @file        dht11.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-09-06
 * @brief       DHT11 数字温湿度传感器驱动代码 (已适配三引脚，引脚改为 PF9)
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 修改说明：
 * 1. 引脚从默认的 PB12 修改为 PF9 (避免 PA1 以太网冲突，提高稳定性)
 * 2. 确保开启 GPIOF 时钟
 ****************************************************************************************************
 */

#ifndef __DHT11_H
#define __DHT11_H 

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/

/* ================= 关键修改区域 ================= */
/* 
   建议使用 PF9 (阿波罗板常用空闲 IO) 
   如果你确信 PA1 没有硬件冲突且接线无误，可改回 GPIOA / GPIO_PIN_1 
*/
#define DHT11_DQ_GPIO_PORT                  GPIOF
#define DHT11_DQ_GPIO_PIN                   GPIO_PIN_9

/* 对应端口的时钟使能宏 */
#define DHT11_DQ_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)
/* ========================================== */

/******************************************************************************************/

/* IO 操作函数 (保持官方开漏模式，兼容三引脚) */
/* 开漏输出 (OUTPUT_OD) + 上拉 (PULLUP) 是三引脚 DHT11 的最佳配置 */
#define DHT11_DQ_OUT(x)     do{ (x) ? \
                                HAL_GPIO_WritePin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN, GPIO_PIN_SET) : \
                                HAL_GPIO_WritePin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN, GPIO_PIN_RESET); \
                            }while(0)

#define DHT11_DQ_IN         HAL_GPIO_ReadPin(DHT11_DQ_GPIO_PORT, DHT11_DQ_GPIO_PIN)

/******************************************************************************************/

uint8_t dht11_init(void);                                /* 初始化 DHT11 */
uint8_t dht11_check(void);                               /* 检测是否存在 DHT11 */
uint8_t dht11_read_data(uint8_t *temp, uint8_t *humi);   /* 读取温湿度 */

#endif