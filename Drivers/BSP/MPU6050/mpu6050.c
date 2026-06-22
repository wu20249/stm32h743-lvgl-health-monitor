#include "./BSP/MPU6050/mpu6050.h"
#include "./SYSTEM/delay/delay.h"
#include "string.h"

/******************************************************************************************************/
/*                                      PH4 / PH5 软件 IIC 引脚定义                                    */
/******************************************************************************************************/

/*
 * PH4 -> IIC_SCL
 * PH5 -> IIC_SDA
 */
#define MPU_IIC_SCL_GPIO_PORT       GPIOH
#define MPU_IIC_SCL_GPIO_PIN        GPIO_PIN_4

#define MPU_IIC_SDA_GPIO_PORT       GPIOH
#define MPU_IIC_SDA_GPIO_PIN        GPIO_PIN_5

/*
 * MPU6050 寄存器地址定义
 */
#define MPU6050_REG_SMPLRT_DIV      0x19    /* 采样率分频寄存器 */
#define MPU6050_REG_CONFIG          0x1A    /* 配置寄存器，主要配置低通滤波 */
#define MPU6050_REG_GYRO_CONFIG     0x1B    /* 陀螺仪量程配置寄存器 */
#define MPU6050_REG_ACCEL_CONFIG    0x1C    /* 加速度计量程配置寄存器 */
#define MPU6050_REG_ACCEL_XOUT_H    0x3B    /* 加速度 X 轴高 8 位寄存器 */
#define MPU6050_REG_PWR_MGMT_1      0x6B    /* 电源管理寄存器 1 */
#define MPU6050_REG_WHO_AM_I        0x75    /* 设备 ID 寄存器 */

/* ================= 内部函数声明 ================= */

static void MPU_IIC_SCL(uint8_t state);
static void MPU_IIC_SDA(uint8_t state);
static uint8_t MPU_IIC_READ_SDA(void);

static void MPU_IIC_SDA_OUT(void);
static void MPU_IIC_SDA_IN(void);
static void MPU_IIC_Delay(void);

static void MPU_IIC_Init(void);
static void MPU_IIC_Start(void);
static void MPU_IIC_Stop(void);
static uint8_t MPU_IIC_Wait_Ack(void);
static void MPU_IIC_Ack(void);
static void MPU_IIC_NAck(void);
static void MPU_IIC_Send_Byte(uint8_t data);
static uint8_t MPU_IIC_Read_Byte(uint8_t ack);

static uint8_t MPU6050_Write_Reg(uint8_t reg, uint8_t data);
static uint8_t MPU6050_Read_Reg(uint8_t reg, uint8_t *data);
static uint8_t MPU6050_Read_Len(uint8_t reg, uint8_t *buf, uint8_t len);

/**
 * @brief   检测指定 IIC 地址是否有设备应答
 * @param   addr：7 位 IIC 地址，例如 MPU6050 常见地址 0x68 或 0x69
 * @retval  0：有设备应答
 *          1：没有设备应答
 * @note    这个函数用于判断 IIC 总线上某个地址是否存在设备
 */
uint8_t MPU6050_IIC_CheckAddr(uint8_t addr)
{
    uint8_t res;

    MPU_IIC_Init();

    MPU_IIC_Start();

    MPU_IIC_Send_Byte((addr << 1) | 0);

    res = MPU_IIC_Wait_Ack();

    MPU_IIC_Stop();

    if (res == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


/**
 * @brief   扫描 IIC 总线，寻找第一个有应答的设备地址
 * @param   addr：保存扫描到的第一个 IIC 设备地址
 * @retval  0：扫描到设备
 *          1：没有扫描到任何设备
 * @note    如果 PH4/PH5 这条 IIC 总线正常，通常会扫到板载 EEPROM、IO扩展、音频芯片等设备
 */
uint8_t MPU6050_IIC_Scan_First(uint8_t *addr)
{
    uint8_t i;

    MPU_IIC_Init();

    for (i = 1; i < 127; i++)
    {
        if (MPU6050_IIC_CheckAddr(i) == 0)
        {
            *addr = i;
            return 0;
        }
    }

    return 1;
}

/**
 * @brief   设置 SCL 引脚电平
 * @param   state：0 输出低电平，1 输出高电平
 * @retval  无
 */
static void MPU_IIC_SCL(uint8_t state)
{
    HAL_GPIO_WritePin(MPU_IIC_SCL_GPIO_PORT,
                      MPU_IIC_SCL_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/**
 * @brief   设置 SDA 引脚电平
 * @param   state：0 输出低电平，1 输出高电平
 * @retval  无
 */
static void MPU_IIC_SDA(uint8_t state)
{
    HAL_GPIO_WritePin(MPU_IIC_SDA_GPIO_PORT,
                      MPU_IIC_SDA_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/**
 * @brief   读取 SDA 引脚当前电平
 * @param   无
 * @retval  0：SDA 为低电平
 *          1：SDA 为高电平
 */
static uint8_t MPU_IIC_READ_SDA(void)
{
    return HAL_GPIO_ReadPin(MPU_IIC_SDA_GPIO_PORT, MPU_IIC_SDA_GPIO_PIN);
}


/**
 * @brief   将 SDA 配置为输出模式
 * @param   无
 * @retval  无
 * @note    软件 IIC 在发送数据和发送 ACK 时，需要 SDA 为输出
 */
static void MPU_IIC_SDA_OUT(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = MPU_IIC_SDA_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;       /* 开漏输出，符合 IIC 总线特性 */
    gpio_init_struct.Pull = GPIO_PULLUP;               /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(MPU_IIC_SDA_GPIO_PORT, &gpio_init_struct);
}


/**
 * @brief   将 SDA 配置为输入模式
 * @param   无
 * @retval  无
 * @note    软件 IIC 在读取数据和等待从机 ACK 时，需要 SDA 为输入
 */
static void MPU_IIC_SDA_IN(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = MPU_IIC_SDA_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;           /* 输入模式 */
    gpio_init_struct.Pull = GPIO_PULLUP;               /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(MPU_IIC_SDA_GPIO_PORT, &gpio_init_struct);
}


/**
 * @brief   软件 IIC 简短延时
 * @param   无
 * @retval  无
 * @note    用于控制 IIC 时序速度
 */
static void MPU_IIC_Delay(void)
{
    delay_us(5);
}


/**
 * @brief   初始化软件 IIC 引脚
 * @param   无
 * @retval  无
 * @note    初始化 PH4 为 SCL，PH5 为 SDA
 */
static void MPU_IIC_Init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_GPIOH_CLK_ENABLE();

    gpio_init_struct.Pin = MPU_IIC_SCL_GPIO_PIN | MPU_IIC_SDA_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;       /* IIC 使用开漏输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;               /* IIC 总线需要上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(GPIOH, &gpio_init_struct);

    MPU_IIC_SCL(1);
    MPU_IIC_SDA(1);
}


/**
 * @brief   产生 IIC 起始信号
 * @param   无
 * @retval  无
 * @note    起始信号：SCL 为高电平时，SDA 从高电平变为低电平
 */
static void MPU_IIC_Start(void)
{
    MPU_IIC_SDA_OUT();

    MPU_IIC_SDA(1);
    MPU_IIC_SCL(1);
    MPU_IIC_Delay();

    MPU_IIC_SDA(0);
    MPU_IIC_Delay();

    MPU_IIC_SCL(0);
    MPU_IIC_Delay();
}


/**
 * @brief   产生 IIC 停止信号
 * @param   无
 * @retval  无
 * @note    停止信号：SCL 为高电平时，SDA 从低电平变为高电平
 */
static void MPU_IIC_Stop(void)
{
    MPU_IIC_SDA_OUT();

    MPU_IIC_SCL(0);
    MPU_IIC_SDA(0);
    MPU_IIC_Delay();

    MPU_IIC_SCL(1);
    MPU_IIC_Delay();

    MPU_IIC_SDA(1);
    MPU_IIC_Delay();
}


/**
 * @brief   等待 IIC 从机应答 ACK
 * @param   无
 * @retval  0：收到 ACK
 *          1：没有收到 ACK
 * @note    ACK 表示从机把 SDA 拉低
 */
static uint8_t MPU_IIC_Wait_Ack(void)
{
    uint8_t wait_time = 0;

    MPU_IIC_SDA_OUT();
    MPU_IIC_SDA(1);        /* 释放 SDA */
    MPU_IIC_Delay();

    MPU_IIC_SDA_IN();

    MPU_IIC_SCL(1);
    MPU_IIC_Delay();

    while (MPU_IIC_READ_SDA())
    {
        wait_time++;

        if (wait_time > 250)
        {
            MPU_IIC_Stop();
            return 1;
        }

        delay_us(1);
    }

    MPU_IIC_SCL(0);
    MPU_IIC_Delay();

    return 0;
}


/**
 * @brief   主机产生 ACK 应答信号
 * @param   无
 * @retval  无
 * @note    主机读取一个字节后，如果还要继续读，就发送 ACK
 */
static void MPU_IIC_Ack(void)
{
    MPU_IIC_SCL(0);
    MPU_IIC_SDA_OUT();

    MPU_IIC_SDA(0);
    MPU_IIC_Delay();

    MPU_IIC_SCL(1);
    MPU_IIC_Delay();

    MPU_IIC_SCL(0);
    MPU_IIC_Delay();
}


/**
 * @brief   主机产生 NACK 非应答信号
 * @param   无
 * @retval  无
 * @note    主机读取最后一个字节后，发送 NACK 表示读取结束
 */
static void MPU_IIC_NAck(void)
{
    MPU_IIC_SCL(0);
    MPU_IIC_SDA_OUT();

    MPU_IIC_SDA(1);
    MPU_IIC_Delay();

    MPU_IIC_SCL(1);
    MPU_IIC_Delay();

    MPU_IIC_SCL(0);
    MPU_IIC_Delay();
}


/**
 * @brief   IIC 发送一个字节
 * @param   data：要发送的 8 位数据
 * @retval  无
 * @note    从高位到低位依次发送
 */
static void MPU_IIC_Send_Byte(uint8_t data)
{
    uint8_t i;

    MPU_IIC_SDA_OUT();

    MPU_IIC_SCL(0);

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            MPU_IIC_SDA(1);
        }
        else
        {
            MPU_IIC_SDA(0);
        }

        data <<= 1;

        MPU_IIC_Delay();

        MPU_IIC_SCL(1);
        MPU_IIC_Delay();

        MPU_IIC_SCL(0);
        MPU_IIC_Delay();
    }
}


/**
 * @brief   IIC 读取一个字节
 * @param   ack：1 表示读取后发送 ACK
 *               0 表示读取后发送 NACK
 * @retval  读取到的 8 位数据
 */
static uint8_t MPU_IIC_Read_Byte(uint8_t ack)
{
    uint8_t i;
    uint8_t receive = 0;

    MPU_IIC_SDA_IN();

    for (i = 0; i < 8; i++)
    {
        receive <<= 1;

        MPU_IIC_SCL(0);
        MPU_IIC_Delay();

        MPU_IIC_SCL(1);
        MPU_IIC_Delay();

        if (MPU_IIC_READ_SDA())
        {
            receive++;
        }
    }

    MPU_IIC_SCL(0);

    if (ack)
    {
        MPU_IIC_Ack();
    }
    else
    {
        MPU_IIC_NAck();
    }

    return receive;
}


/**
 * @brief   向 MPU6050 指定寄存器写入一个字节
 * @param   reg：寄存器地址
 * @param   data：要写入的数据
 * @retval  0：写入成功
 *          1：写入失败
 */
static uint8_t MPU6050_Write_Reg(uint8_t reg, uint8_t data)
{
    MPU_IIC_Start();

    MPU_IIC_Send_Byte((MPU6050_ADDR << 1) | 0);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Send_Byte(reg);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Send_Byte(data);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Stop();

    return 0;
}


/**
 * @brief   从 MPU6050 指定寄存器读取一个字节
 * @param   reg：寄存器地址
 * @param   data：用于保存读取到的数据
 * @retval  0：读取成功
 *          1：读取失败
 */
static uint8_t MPU6050_Read_Reg(uint8_t reg, uint8_t *data)
{
    MPU_IIC_Start();

    MPU_IIC_Send_Byte((MPU6050_ADDR << 1) | 0);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Send_Byte(reg);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Start();

    MPU_IIC_Send_Byte((MPU6050_ADDR << 1) | 1);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    *data = MPU_IIC_Read_Byte(0);

    MPU_IIC_Stop();

    return 0;
}


/**
 * @brief   从 MPU6050 指定寄存器开始连续读取多个字节
 * @param   reg：起始寄存器地址
 * @param   buf：数据保存缓冲区
 * @param   len：读取字节数
 * @retval  0：读取成功
 *          1：读取失败
 */
static uint8_t MPU6050_Read_Len(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    MPU_IIC_Start();

    MPU_IIC_Send_Byte((MPU6050_ADDR << 1) | 0);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Send_Byte(reg);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    MPU_IIC_Start();

    MPU_IIC_Send_Byte((MPU6050_ADDR << 1) | 1);
    if (MPU_IIC_Wait_Ack())
    {
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        if (i != (len - 1))
        {
            buf[i] = MPU_IIC_Read_Byte(1);
        }
        else
        {
            buf[i] = MPU_IIC_Read_Byte(0);
        }
    }

    MPU_IIC_Stop();

    return 0;
}


/**
 * @brief   获取 MPU6050 的 WHO_AM_I ID
 * @param   id：用于保存读取到的 ID
 * @retval  0：读取成功
 *          1：读取失败
 * @note    正常情况下，MPU6050 的 ID 应该是 0x68
 */
uint8_t MPU6050_Get_ID(uint8_t *id)
{
    if (id == NULL)
    {
        return 1;
    }

    /*
     * 这里必须初始化 IIC
     * 因为 task_mpu6050 可能会直接调用 MPU6050_Get_ID()
     */
    MPU_IIC_Init();

    delay_ms(10);

    return MPU6050_Read_Reg(MPU6050_REG_WHO_AM_I, id);
}


/**
 * @brief   初始化 MPU6050
 * @param   无
 * @retval  0：初始化成功
 *          1：读取 WHO_AM_I 失败
 *          2：WHO_AM_I 不是 0x68
 *          3：复位 MPU6050 失败
 *          4：唤醒 MPU6050 失败
 *          5：配置 MPU6050 失败
 */
uint8_t MPU6050_Init(void)
{
    uint8_t id;

    MPU_IIC_Init();

    delay_ms(100);

    /*
     * 读取 WHO_AM_I
     */
    if (MPU6050_Read_Reg(MPU6050_REG_WHO_AM_I, &id))
    {
        return 1;
    }

    /*
     * 正常 MPU6050 的 WHO_AM_I 应该是 0x68
     */
    if ((id != 0x68) && (id != 0x70) && (id != 0x71))
    {
    return 2;
    }

    /*
     * 复位 MPU6050
     */
    if (MPU6050_Write_Reg(MPU6050_REG_PWR_MGMT_1, 0x80))
    {
        return 3;
    }

    delay_ms(100);

    /*
     * 唤醒 MPU6050
     */
    if (MPU6050_Write_Reg(MPU6050_REG_PWR_MGMT_1, 0x00))
    {
        return 4;
    }

    delay_ms(10);

    /*
     * 设置采样率分频
     */
    if (MPU6050_Write_Reg(MPU6050_REG_SMPLRT_DIV, 0x09))
    {
        return 5;
    }

    /*
     * 设置低通滤波
     */
    if (MPU6050_Write_Reg(MPU6050_REG_CONFIG, 0x06))
    {
        return 5;
    }

    /*
     * 陀螺仪量程 ±2000 °/s
     */
    if (MPU6050_Write_Reg(MPU6050_REG_GYRO_CONFIG, 0x18))
    {
        return 5;
    }

    /*
     * 加速度量程 ±2g
     */
    if (MPU6050_Write_Reg(MPU6050_REG_ACCEL_CONFIG, 0x00))
    {
        return 5;
    }

    return 0;
}


/**
 * @brief   读取 MPU6050 的原始数据
 * @param   data：MPU6050_Data_t 结构体指针，用于保存原始数据
 * @retval  0：读取成功
 *          1：读取失败
 * @note    一次连续读取 14 个字节，包括加速度、温度、陀螺仪
 */
uint8_t MPU6050_Read_RawData(MPU6050_Data_t *data)
{
    uint8_t buf[14];

    if (data == NULL)
    {
        return 1;
    }

    if (MPU6050_Read_Len(MPU6050_REG_ACCEL_XOUT_H, buf, 14))
    {
        return 1;
    }

    data->acc_x_raw  = (int16_t)((buf[0]  << 8) | buf[1]);
    data->acc_y_raw  = (int16_t)((buf[2]  << 8) | buf[3]);
    data->acc_z_raw  = (int16_t)((buf[4]  << 8) | buf[5]);

    data->temp_raw   = (int16_t)((buf[6]  << 8) | buf[7]);

    data->gyro_x_raw = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gyro_y_raw = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z_raw = (int16_t)((buf[12] << 8) | buf[13]);

    return 0;
}


/**
 * @brief   读取 MPU6050 数据，并转换为实际单位
 * @param   data：MPU6050_Data_t 结构体指针，用于保存原始值和转换后的值
 * @retval  0：读取成功
 *          1：读取失败
 * @note    加速度单位为 g，角速度单位为 °/s，温度单位为 ℃
 */
uint8_t MPU6050_Read_Data(MPU6050_Data_t *data)
{
    if (MPU6050_Read_RawData(data))
    {
        return 1;
    }

    /*
     * 加速度计当前量程是 ±2g
     * 灵敏度是 16384 LSB/g
     */
    data->acc_x_g = data->acc_x_raw / 16384.0f;
    data->acc_y_g = data->acc_y_raw / 16384.0f;
    data->acc_z_g = data->acc_z_raw / 16384.0f;

    /*
     * 陀螺仪当前量程是 ±2000 °/s
     * 灵敏度是 16.4 LSB/(°/s)
     */
    data->gyro_x_dps = data->gyro_x_raw / 16.4f;
    data->gyro_y_dps = data->gyro_y_raw / 16.4f;
    data->gyro_z_dps = data->gyro_z_raw / 16.4f;

    /*
     * MPU6050 内部温度计算公式
     */
    data->temperature = data->temp_raw / 340.0f + 36.53f;

    return 0;
}