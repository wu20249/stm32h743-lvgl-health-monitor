#ifndef __MAX30102_H
#define __MAX30102_H

#include "./SYSTEM/sys/sys.h"

/*
 * MAX30102 默认 IIC 地址
 * MAX30102 的 7 位 IIC 地址通常固定为 0x57
 */
#define MAX30102_ADDR              0x57

/*
 * MAX30102 主要寄存器地址
 */
#define MAX30102_REG_INTR_STATUS_1 0x00    /* 中断状态寄存器 1 */
#define MAX30102_REG_INTR_STATUS_2 0x01    /* 中断状态寄存器 2 */
#define MAX30102_REG_INTR_ENABLE_1 0x02    /* 中断使能寄存器 1 */
#define MAX30102_REG_INTR_ENABLE_2 0x03    /* 中断使能寄存器 2 */

#define MAX30102_REG_FIFO_WR_PTR   0x04    /* FIFO 写指针 */
#define MAX30102_REG_OVF_COUNTER   0x05    /* FIFO 溢出计数器 */
#define MAX30102_REG_FIFO_RD_PTR   0x06    /* FIFO 读指针 */
#define MAX30102_REG_FIFO_DATA     0x07    /* FIFO 数据寄存器 */
#define MAX30102_REG_FIFO_CONFIG   0x08    /* FIFO 配置寄存器 */

#define MAX30102_REG_MODE_CONFIG   0x09    /* 模式配置寄存器 */
#define MAX30102_REG_SPO2_CONFIG   0x0A    /* 血氧配置寄存器 */

#define MAX30102_REG_LED1_PA       0x0C    /* LED1 电流配置，通常是 RED */
#define MAX30102_REG_LED2_PA       0x0D    /* LED2 电流配置，通常是 IR */

#define MAX30102_REG_REV_ID        0xFE    /* 版本 ID 寄存器 */
#define MAX30102_REG_PART_ID       0xFF    /* 芯片 ID 寄存器 */


/*
 * MAX30102 数据结构体
 * 用于保存一次 FIFO 读取到的红光 RED 和红外 IR 原始数据
 */
typedef struct
{
    uint32_t red;      /* 红光原始数据 */
    uint32_t ir;       /* 红外原始数据 */
} MAX30102_Data_t;

/*
 * MAX30102 心率血氧算法相关参数
 */
#define MAX30102_FINGER_THRESHOLD    30000
#define MAX30102_SAMPLE_RATE_HZ      50
#define MAX30102_SAMPLE_COUNT        150

/*
 * MAX30102 心率血氧结果结构体
 */
typedef struct
{
    uint8_t heart_rate;
    uint8_t spo2;
    uint8_t heart_valid;
    uint8_t spo2_valid;
} MAX30102_Result_t;


/**
 * @brief   根据 IR 原始值判断是否有手指
 * @param   ir：IR 原始数据
 * @retval  0：无手指
 *          1：有手指
 */
uint8_t MAX30102_Check_Finger(uint32_t ir);


/**
 * @brief   根据 IR 数据计算心率
 * @param   ir_buf：IR 原始数据数组
 * @param   len：数据长度
 * @param   sample_rate：采样率
 * @param   heart_rate：保存心率结果
 * @retval  0：成功
 *          1：失败
 */
uint8_t MAX30102_Calc_HeartRate(uint32_t *ir_buf,
                                uint16_t len,
                                uint8_t sample_rate,
                                uint8_t *heart_rate);


/**
 * @brief   根据 RED / IR 数据计算血氧
 * @param   red_buf：RED 原始数据数组
 * @param   ir_buf：IR 原始数据数组
 * @param   len：数据长度
 * @param   spo2：保存血氧结果
 * @retval  0：成功
 *          1：失败
 */
uint8_t MAX30102_Calc_SpO2(uint32_t *red_buf,
                           uint32_t *ir_buf,
                           uint16_t len,
                           uint8_t *spo2);


/**
 * @brief   同时计算心率和血氧
 * @param   red_buf：RED 原始数据数组
 * @param   ir_buf：IR 原始数据数组
 * @param   len：数据长度
 * @param   sample_rate：采样率
 * @param   result：保存计算结果
 * @retval  0：成功
 *          1：失败
 */
uint8_t MAX30102_Calc_HR_SpO2(uint32_t *red_buf,
                              uint32_t *ir_buf,
                              uint16_t len,
                              uint8_t sample_rate,
                              MAX30102_Result_t *result);


/**
 * @brief   初始化 MAX30102 使用的软件 IIC 引脚
 * @param   无
 * @retval  无
 * @note    当前配置为 PG13 = SDA，PG14 = SCL
 */
void MAX30102_IIC_Init(void);


/**
 * @brief   检测指定 IIC 地址是否有设备应答
 * @param   addr：7 位 IIC 地址，例如 MAX30102 是 0x57
 * @retval  0：有设备应答
 *          1：没有设备应答
 */
uint8_t MAX30102_IIC_CheckAddr(uint8_t addr);


/**
 * @brief   读取 MAX30102 的 PART_ID
 * @param   id：用于保存读取到的 PART_ID
 * @retval  0：读取成功
 *          1：读取失败
 * @note    MAX30102 常见 PART_ID 为 0x15
 */
uint8_t MAX30102_Get_PartID(uint8_t *id);


/**
 * @brief   初始化 MAX30102
 * @param   无
 * @retval  0：初始化成功
 *          1：IIC 地址 0x57 无应答
 *          2：PART_ID 读取失败
 *          3：PART_ID 不是预期值
 *          4：MAX30102 复位失败
 *          5：MAX30102 配置失败
 */
uint8_t MAX30102_Init(void);


/**
 * @brief   从 MAX30102 FIFO 中读取一次 RED / IR 原始数据
 * @param   data：MAX30102_Data_t 结构体指针，用于保存 RED 和 IR 原始值
 * @retval  0：读取成功
 *          1：读取失败
 * @note    这里只读取原始 RED / IR 数据，还没有计算心率和血氧
 */
uint8_t MAX30102_Read_FIFO(MAX30102_Data_t *data);

#endif