#ifndef __MPU6050_H
#define __MPU6050_H

#include "./SYSTEM/sys/sys.h"

/*
 * MPU6050 默认 I2C 地址：
 * AD0 接 GND 时地址为 0x68
 * AD0 接 VCC 时地址为 0x69
 */
#define MPU6050_ADDR        0x68

/*
 * MPU6050 数据结构体
 * 用于保存一次读取到的加速度、角速度和温度数据
 */
typedef struct
{
    int16_t acc_x_raw;        /* 加速度 X 轴原始值 */
    int16_t acc_y_raw;        /* 加速度 Y 轴原始值 */
    int16_t acc_z_raw;        /* 加速度 Z 轴原始值 */

    int16_t gyro_x_raw;       /* 陀螺仪 X 轴原始值 */
    int16_t gyro_y_raw;       /* 陀螺仪 Y 轴原始值 */
    int16_t gyro_z_raw;       /* 陀螺仪 Z 轴原始值 */

    int16_t temp_raw;         /* 温度原始值 */

    float acc_x_g;            /* 加速度 X 轴，单位 g */
    float acc_y_g;            /* 加速度 Y 轴，单位 g */
    float acc_z_g;            /* 加速度 Z 轴，单位 g */

    float gyro_x_dps;         /* 角速度 X 轴，单位 °/s */
    float gyro_y_dps;         /* 角速度 Y 轴，单位 °/s */
    float gyro_z_dps;         /* 角速度 Z 轴，单位 °/s */

    float temperature;        /* MPU6050 内部温度，单位 ℃ */
} MPU6050_Data_t;


/**
 * @brief   初始化 MPU6050
 * @param   无
 * @retval  0：初始化成功
 *          1：初始化失败，可能是接线错误、IIC通信失败或没有检测到 MPU6050
 */
uint8_t MPU6050_Init(void);


/**
 * @brief   获取 MPU6050 的 WHO_AM_I ID
 * @param   id：用于保存读取到的 ID
 * @retval  0：读取成功
 *          1：读取失败
 */
uint8_t MPU6050_Get_ID(uint8_t *id);


/**
 * @brief   读取 MPU6050 的原始数据
 * @param   data：MPU6050_Data_t 结构体指针，用于保存原始数据
 * @retval  0：读取成功
 *          1：读取失败
 */
uint8_t MPU6050_Read_RawData(MPU6050_Data_t *data);


/**
 * @brief   读取 MPU6050 数据，并转换成实际单位
 * @param   data：MPU6050_Data_t 结构体指针，用于保存转换后的数据
 * @retval  0：读取成功
 *          1：读取失败
 */
uint8_t MPU6050_Read_Data(MPU6050_Data_t *data);

/**
 * @brief   检测指定 IIC 地址是否有设备应答
 * @param   addr：7 位 IIC 地址，比如 0x68、0x69
 * @retval  0：该地址有设备应答
 *          1：该地址没有设备应答
 */
uint8_t MPU6050_IIC_CheckAddr(uint8_t addr);


/**
 * @brief   扫描 IIC 总线，寻找第一个有应答的设备地址
 * @param   addr：用于保存扫描到的第一个设备地址
 * @retval  0：扫描到设备
 *          1：没有扫描到任何设备
 */
uint8_t MPU6050_IIC_Scan_First(uint8_t *addr);

#endif