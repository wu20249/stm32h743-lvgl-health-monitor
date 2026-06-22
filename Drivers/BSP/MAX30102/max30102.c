#include "./BSP/MAX30102/max30102.h"
#include "./SYSTEM/delay/delay.h"
#include "string.h"

/******************************************************************************************************/
/*                                      PG13 / PG14 软件 IIC 引脚定义                                  */
/******************************************************************************************************/

#define MAX30102_SDA_GPIO_PORT      GPIOG
#define MAX30102_SDA_GPIO_PIN       GPIO_PIN_13

#define MAX30102_SCL_GPIO_PORT      GPIOG
#define MAX30102_SCL_GPIO_PIN       GPIO_PIN_14


/******************************************************************************************************/
/*                                      内部函数声明                                                   */
/******************************************************************************************************/

static void MAX30102_IIC_SDA(uint8_t state);
static void MAX30102_IIC_SCL(uint8_t state);
static uint8_t MAX30102_IIC_READ_SDA(void);

static void MAX30102_IIC_SDA_OUT(void);
static void MAX30102_IIC_SDA_IN(void);
static void MAX30102_IIC_Delay(void);

static void MAX30102_IIC_Start(void);
static void MAX30102_IIC_Stop(void);
static uint8_t MAX30102_IIC_Wait_Ack(void);
static void MAX30102_IIC_Ack(void);
static void MAX30102_IIC_NAck(void);
static void MAX30102_IIC_Send_Byte(uint8_t data);
static uint8_t MAX30102_IIC_Read_Byte(uint8_t ack);

static uint8_t MAX30102_Write_Reg(uint8_t reg, uint8_t data);
static uint8_t MAX30102_Read_Reg(uint8_t reg, uint8_t *data);
static uint8_t MAX30102_Read_Len(uint8_t reg, uint8_t *buf, uint8_t len);


/**
 * @brief   设置 SDA 引脚电平
 * @param   state：0 输出低电平，1 输出高电平
 * @retval  无
 */
static void MAX30102_IIC_SDA(uint8_t state)
{
    HAL_GPIO_WritePin(MAX30102_SDA_GPIO_PORT,
                      MAX30102_SDA_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/**
 * @brief   设置 SCL 引脚电平
 * @param   state：0 输出低电平，1 输出高电平
 * @retval  无
 */
static void MAX30102_IIC_SCL(uint8_t state)
{
    HAL_GPIO_WritePin(MAX30102_SCL_GPIO_PORT,
                      MAX30102_SCL_GPIO_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/**
 * @brief   读取 SDA 引脚电平
 * @param   无
 * @retval  0：SDA 当前为低电平
 *          1：SDA 当前为高电平
 */
static uint8_t MAX30102_IIC_READ_SDA(void)
{
    return HAL_GPIO_ReadPin(MAX30102_SDA_GPIO_PORT, MAX30102_SDA_GPIO_PIN);
}


/**
 * @brief   将 SDA 配置为输出模式
 * @param   无
 * @retval  无
 * @note    软件 IIC 发送数据和发送 ACK/NACK 时需要 SDA 输出
 */
static void MAX30102_IIC_SDA_OUT(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = MAX30102_SDA_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(MAX30102_SDA_GPIO_PORT, &gpio_init_struct);
}


/**
 * @brief   将 SDA 配置为输入模式
 * @param   无
 * @retval  无
 * @note    软件 IIC 读取数据和等待从机 ACK 时需要 SDA 输入
 */
static void MAX30102_IIC_SDA_IN(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = MAX30102_SDA_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(MAX30102_SDA_GPIO_PORT, &gpio_init_struct);
}


/**
 * @brief   软件 IIC 短延时
 * @param   无
 * @retval  无
 * @note    控制软件 IIC 时序速度
 */
static void MAX30102_IIC_Delay(void)
{
    delay_us(5);
}


/**
 * @brief   初始化 MAX30102 软件 IIC 引脚
 * @param   无
 * @retval  无
 * @note    PG13 配置为 SDA，PG14 配置为 SCL，均为开漏输出
 */
void MAX30102_IIC_Init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio_init_struct.Pin = MAX30102_SDA_GPIO_PIN | MAX30102_SCL_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(GPIOG, &gpio_init_struct);

    MAX30102_IIC_SDA(1);
    MAX30102_IIC_SCL(1);
}


/**
 * @brief   产生 IIC 起始信号
 * @param   无
 * @retval  无
 * @note    起始信号：SCL 为高电平时，SDA 从高电平变为低电平
 */
static void MAX30102_IIC_Start(void)
{
    MAX30102_IIC_SDA_OUT();

    MAX30102_IIC_SDA(1);
    MAX30102_IIC_SCL(1);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SDA(0);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SCL(0);
    MAX30102_IIC_Delay();
}


/**
 * @brief   产生 IIC 停止信号
 * @param   无
 * @retval  无
 * @note    停止信号：SCL 为高电平时，SDA 从低电平变为高电平
 */
static void MAX30102_IIC_Stop(void)
{
    MAX30102_IIC_SDA_OUT();

    MAX30102_IIC_SCL(0);
    MAX30102_IIC_SDA(0);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SCL(1);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SDA(1);
    MAX30102_IIC_Delay();
}


/**
 * @brief   等待 IIC 从机 ACK 应答
 * @param   无
 * @retval  0：收到 ACK
 *          1：没有收到 ACK
 * @note    ACK 表示从机在第 9 个时钟周期把 SDA 拉低
 */
static uint8_t MAX30102_IIC_Wait_Ack(void)
{
    uint8_t wait_time = 0;

    MAX30102_IIC_SDA_OUT();
    MAX30102_IIC_SDA(1);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SDA_IN();

    MAX30102_IIC_SCL(1);
    MAX30102_IIC_Delay();

    while (MAX30102_IIC_READ_SDA())
    {
        wait_time++;

        if (wait_time > 250)
        {
            MAX30102_IIC_Stop();
            return 1;
        }

        delay_us(1);
    }

    MAX30102_IIC_SCL(0);
    MAX30102_IIC_Delay();

    return 0;
}


/**
 * @brief   主机发送 ACK 应答
 * @param   无
 * @retval  无
 * @note    主机读取多个字节时，非最后一个字节后发送 ACK
 */
static void MAX30102_IIC_Ack(void)
{
    MAX30102_IIC_SCL(0);
    MAX30102_IIC_SDA_OUT();

    MAX30102_IIC_SDA(0);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SCL(1);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SCL(0);
    MAX30102_IIC_Delay();
}


/**
 * @brief   主机发送 NACK 非应答
 * @param   无
 * @retval  无
 * @note    主机读取最后一个字节后发送 NACK，表示读取结束
 */
static void MAX30102_IIC_NAck(void)
{
    MAX30102_IIC_SCL(0);
    MAX30102_IIC_SDA_OUT();

    MAX30102_IIC_SDA(1);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SCL(1);
    MAX30102_IIC_Delay();

    MAX30102_IIC_SCL(0);
    MAX30102_IIC_Delay();
}


/**
 * @brief   IIC 发送一个字节
 * @param   data：要发送的 8 位数据
 * @retval  无
 * @note    从最高位开始发送
 */
static void MAX30102_IIC_Send_Byte(uint8_t data)
{
    uint8_t i;

    MAX30102_IIC_SDA_OUT();

    MAX30102_IIC_SCL(0);

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            MAX30102_IIC_SDA(1);
        }
        else
        {
            MAX30102_IIC_SDA(0);
        }

        data <<= 1;

        MAX30102_IIC_Delay();

        MAX30102_IIC_SCL(1);
        MAX30102_IIC_Delay();

        MAX30102_IIC_SCL(0);
        MAX30102_IIC_Delay();
    }
}


/**
 * @brief   IIC 读取一个字节
 * @param   ack：1 表示读取后发送 ACK
 *               0 表示读取后发送 NACK
 * @retval  读取到的 8 位数据
 */
static uint8_t MAX30102_IIC_Read_Byte(uint8_t ack)
{
    uint8_t i;
    uint8_t receive = 0;

    MAX30102_IIC_SDA_IN();

    for (i = 0; i < 8; i++)
    {
        receive <<= 1;

        MAX30102_IIC_SCL(0);
        MAX30102_IIC_Delay();

        MAX30102_IIC_SCL(1);
        MAX30102_IIC_Delay();

        if (MAX30102_IIC_READ_SDA())
        {
            receive++;
        }
    }

    MAX30102_IIC_SCL(0);

    if (ack)
    {
        MAX30102_IIC_Ack();
    }
    else
    {
        MAX30102_IIC_NAck();
    }

    return receive;
}


/**
 * @brief   向 MAX30102 指定寄存器写入一个字节
 * @param   reg：寄存器地址
 * @param   data：要写入的数据
 * @retval  0：写入成功
 *          1：写入失败
 */
static uint8_t MAX30102_Write_Reg(uint8_t reg, uint8_t data)
{
    MAX30102_IIC_Start();

    MAX30102_IIC_Send_Byte((MAX30102_ADDR << 1) | 0);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Send_Byte(reg);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Send_Byte(data);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Stop();

    return 0;
}


/**
 * @brief   从 MAX30102 指定寄存器读取一个字节
 * @param   reg：寄存器地址
 * @param   data：保存读取结果的指针
 * @retval  0：读取成功
 *          1：读取失败
 */
static uint8_t MAX30102_Read_Reg(uint8_t reg, uint8_t *data)
{
    MAX30102_IIC_Start();

    MAX30102_IIC_Send_Byte((MAX30102_ADDR << 1) | 0);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Send_Byte(reg);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Start();

    MAX30102_IIC_Send_Byte((MAX30102_ADDR << 1) | 1);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    *data = MAX30102_IIC_Read_Byte(0);

    MAX30102_IIC_Stop();

    return 0;
}


/**
 * @brief   从 MAX30102 指定寄存器连续读取多个字节
 * @param   reg：起始寄存器地址
 * @param   buf：读取数据保存缓冲区
 * @param   len：读取字节数
 * @retval  0：读取成功
 *          1：读取失败
 */
static uint8_t MAX30102_Read_Len(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;

    MAX30102_IIC_Start();

    MAX30102_IIC_Send_Byte((MAX30102_ADDR << 1) | 0);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Send_Byte(reg);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    MAX30102_IIC_Start();

    MAX30102_IIC_Send_Byte((MAX30102_ADDR << 1) | 1);
    if (MAX30102_IIC_Wait_Ack())
    {
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        if (i != (len - 1))
        {
            buf[i] = MAX30102_IIC_Read_Byte(1);
        }
        else
        {
            buf[i] = MAX30102_IIC_Read_Byte(0);
        }
    }

    MAX30102_IIC_Stop();

    return 0;
}


/**
 * @brief   检测指定 IIC 地址是否有设备应答
 * @param   addr：7 位 IIC 地址，例如 MAX30102 是 0x57
 * @retval  0：有设备应答
 *          1：没有设备应答
 */
uint8_t MAX30102_IIC_CheckAddr(uint8_t addr)
{
    uint8_t res;

    MAX30102_IIC_Init();

    MAX30102_IIC_Start();

    MAX30102_IIC_Send_Byte((addr << 1) | 0);

    res = MAX30102_IIC_Wait_Ack();

    MAX30102_IIC_Stop();

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
 * @brief   读取 MAX30102 的 PART_ID
 * @param   id：用于保存读取到的 PART_ID
 * @retval  0：读取成功
 *          1：读取失败
 * @note    MAX30102 常见 PART_ID 为 0x15
 */
uint8_t MAX30102_Get_PartID(uint8_t *id)
{
    if (id == NULL)
    {
        return 1;
    }

    MAX30102_IIC_Init();

    delay_ms(10);

    return MAX30102_Read_Reg(MAX30102_REG_PART_ID, id);
}


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
uint8_t MAX30102_Init(void)
{
    uint8_t id;
    uint8_t reg;

    MAX30102_IIC_Init();

    delay_ms(50);

    /*
     * 检测 0x57 地址是否有应答
     */
    if (MAX30102_IIC_CheckAddr(MAX30102_ADDR))
    {
        return 1;
    }

    /*
     * 读取 PART_ID，确认是否是 MAX30102
     */
    if (MAX30102_Get_PartID(&id))
    {
        return 2;
    }

    /*
     * MAX30102 常见 PART_ID 是 0x15
     * 如果你的模块不是 0x15，先把实际值显示出来再判断
     */
    if (id != 0x15)
    {
        return 3;
    }

    /*
     * 复位 MAX30102
     * MODE_CONFIG 的 bit6 写 1 表示复位
     */
    if (MAX30102_Write_Reg(MAX30102_REG_MODE_CONFIG, 0x40))
    {
        return 4;
    }

    delay_ms(100);

    /*
     * 等待复位完成
     * 复位完成后 bit6 会自动清零
     */
    if (MAX30102_Read_Reg(MAX30102_REG_MODE_CONFIG, &reg))
    {
        return 4;
    }

    /*
     * 清空 FIFO 指针
     */
    if (MAX30102_Write_Reg(MAX30102_REG_FIFO_WR_PTR, 0x00))
    {
        return 5;
    }

    if (MAX30102_Write_Reg(MAX30102_REG_OVF_COUNTER, 0x00))
    {
        return 5;
    }

    if (MAX30102_Write_Reg(MAX30102_REG_FIFO_RD_PTR, 0x00))
    {
        return 5;
    }

    /*
     * FIFO 配置
     * 0x4F：
     * sample average = 4
     * FIFO rollover = disabled
     * FIFO almost full = 15
     */
    if (MAX30102_Write_Reg(MAX30102_REG_FIFO_CONFIG, 0x5F))
    {
        return 5;
    }

    /*
     * 模式配置
     * 0x03：SpO2 模式，启用 RED + IR
     */
    if (MAX30102_Write_Reg(MAX30102_REG_MODE_CONFIG, 0x03))
    {
        return 5;
    }

    /*
     * SpO2 配置
     * 0x27：
     * ADC range = 4096nA
     * sample rate = 100Hz
     * pulse width = 411us，18-bit
     */
    if (MAX30102_Write_Reg(MAX30102_REG_SPO2_CONFIG, 0x23))
    {
        return 5;
    }

    /*
     * LED 电流配置
     * LED1 一般是 RED
     * LED2 一般是 IR
     * 0x24 是一个中等偏上的电流值
     */
    if (MAX30102_Write_Reg(MAX30102_REG_LED1_PA, 0x3F))
    {
        return 5;
    }

    if (MAX30102_Write_Reg(MAX30102_REG_LED2_PA, 0x3F))
    {
        return 5;
    }

    /*
     * 清除中断状态
     * 读取中断状态寄存器即可清除
     */
    MAX30102_Read_Reg(MAX30102_REG_INTR_STATUS_1, &reg);
    MAX30102_Read_Reg(MAX30102_REG_INTR_STATUS_2, &reg);

    return 0;
}


/**
 * @brief   从 MAX30102 FIFO 中读取一次 RED / IR 原始数据
 * @param   data：MAX30102_Data_t 结构体指针，用于保存 RED 和 IR 数据
 * @retval  0：读取成功
 *          1：读取失败
 * @note    SpO2 模式下一组数据通常为 6 字节：RED 3 字节 + IR 3 字节
 */
uint8_t MAX30102_Read_FIFO(MAX30102_Data_t *data)
{
    uint8_t buf[6];
    uint32_t red;
    uint32_t ir;

    if (data == NULL)
    {
        return 1;
    }

    /*
     * 从 FIFO_DATA 寄存器连续读取 6 字节
     */
    if (MAX30102_Read_Len(MAX30102_REG_FIFO_DATA, buf, 6))
    {
        return 1;
    }

    /*
     * RED 数据由 3 字节组成，但有效位通常是低 18 位
     */
    red = ((uint32_t)buf[0] << 16) |
          ((uint32_t)buf[1] << 8)  |
          ((uint32_t)buf[2]);

    red &= 0x03FFFF;

    /*
     * IR 数据由 3 字节组成，但有效位通常是低 18 位
     */
    ir = ((uint32_t)buf[3] << 16) |
         ((uint32_t)buf[4] << 8)  |
         ((uint32_t)buf[5]);

    ir &= 0x03FFFF;

    data->red = red;
    data->ir = ir;

    return 0;
}

uint8_t MAX30102_Check_Finger(uint32_t ir)
{
    if (ir >= MAX30102_FINGER_THRESHOLD)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


uint8_t MAX30102_Calc_HeartRate(uint32_t *ir_buf,
                                uint16_t len,
                                uint8_t sample_rate,
                                uint8_t *heart_rate)
{
    uint16_t i;
    uint64_t sum = 0;
    uint32_t mean;
    uint32_t ir_max = 0;
    uint32_t ir_min = 0xFFFFFFFF;
    uint32_t amplitude;
    uint32_t threshold;

    int16_t last_peak = -1;
    uint16_t interval_sum = 0;
    uint8_t interval_count = 0;

    uint16_t min_peak_distance;
    uint32_t prev;
    uint32_t cur;
    uint32_t next;

    float avg_interval;
    float bpm;

    if ((ir_buf == NULL) || (heart_rate == NULL) || (len < 30) || (sample_rate == 0))
    {
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        sum += ir_buf[i];

        if (ir_buf[i] > ir_max)
        {
            ir_max = ir_buf[i];
        }

        if (ir_buf[i] < ir_min)
        {
            ir_min = ir_buf[i];
        }
    }

    mean = (uint32_t)(sum / len);
    amplitude = ir_max - ir_min;

    if (amplitude < 300)
    {
        return 1;
    }

    threshold = mean + (amplitude / 4);

    min_peak_distance = (uint16_t)((sample_rate * 3) / 10);

    if (min_peak_distance < 1)
    {
        min_peak_distance = 1;
    }

    for (i = 2; i < len - 2; i++)
    {
        prev = (ir_buf[i - 2] + ir_buf[i - 1] + ir_buf[i]) / 3;
        cur  = (ir_buf[i - 1] + ir_buf[i] + ir_buf[i + 1]) / 3;
        next = (ir_buf[i] + ir_buf[i + 1] + ir_buf[i + 2]) / 3;

        if ((cur > prev) && (cur >= next) && (cur > threshold))
        {
            if ((last_peak < 0) || ((i - last_peak) >= min_peak_distance))
            {
                if (last_peak >= 0)
                {
                    interval_sum += (i - last_peak);
                    interval_count++;
                }

                last_peak = i;
            }
        }
    }

    if (interval_count == 0)
    {
        return 1;
    }

    avg_interval = (float)interval_sum / (float)interval_count;

    if (avg_interval <= 0.0f)
    {
        return 1;
    }

    bpm = 60.0f * (float)sample_rate / avg_interval;

    if ((bpm < 40.0f) || (bpm > 180.0f))
    {
        return 1;
    }

    *heart_rate = (uint8_t)(bpm + 0.5f);

    return 0;
}


uint8_t MAX30102_Calc_SpO2(uint32_t *red_buf,
                           uint32_t *ir_buf,
                           uint16_t len,
                           uint8_t *spo2)
{
    uint16_t i;

    uint64_t red_sum = 0;
    uint64_t ir_sum = 0;

    uint32_t red_dc;
    uint32_t ir_dc;

    uint32_t red_max = 0;
    uint32_t red_min = 0xFFFFFFFF;

    uint32_t ir_max = 0;
    uint32_t ir_min = 0xFFFFFFFF;

    uint32_t red_ac;
    uint32_t ir_ac;

    float ratio;
    float spo2_f;

    if ((red_buf == NULL) || (ir_buf == NULL) || (spo2 == NULL) || (len < 30))
    {
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        red_sum += red_buf[i];
        ir_sum += ir_buf[i];

        if (red_buf[i] > red_max)
        {
            red_max = red_buf[i];
        }

        if (red_buf[i] < red_min)
        {
            red_min = red_buf[i];
        }

        if (ir_buf[i] > ir_max)
        {
            ir_max = ir_buf[i];
        }

        if (ir_buf[i] < ir_min)
        {
            ir_min = ir_buf[i];
        }
    }

    red_dc = (uint32_t)(red_sum / len);
    ir_dc = (uint32_t)(ir_sum / len);

    red_ac = red_max - red_min;
    ir_ac = ir_max - ir_min;

    if ((red_dc < MAX30102_FINGER_THRESHOLD) || (ir_dc < MAX30102_FINGER_THRESHOLD))
    {
        return 1;
    }

    if ((red_ac < 300) || (ir_ac < 300))
    {
        return 1;
    }

    ratio = ((float)red_ac / (float)red_dc) /
            ((float)ir_ac / (float)ir_dc);

    spo2_f = 110.0f - 25.0f * ratio;

    if (spo2_f > 100.0f)
    {
        spo2_f = 100.0f;
    }

    if (spo2_f < 70.0f)
    {
        spo2_f = 70.0f;
    }

    *spo2 = (uint8_t)(spo2_f + 0.5f);

    return 0;
}


uint8_t MAX30102_Calc_HR_SpO2(uint32_t *red_buf,
                              uint32_t *ir_buf,
                              uint16_t len,
                              uint8_t sample_rate,
                              MAX30102_Result_t *result)
{
    uint8_t ret_hr;
    uint8_t ret_spo2;

    if (result == NULL)
    {
        return 1;
    }

    result->heart_rate = 0;
    result->spo2 = 0;
    result->heart_valid = 0;
    result->spo2_valid = 0;

    ret_hr = MAX30102_Calc_HeartRate(ir_buf,
                                     len,
                                     sample_rate,
                                     &result->heart_rate);

    if (ret_hr == 0)
    {
        result->heart_valid = 1;
    }

    ret_spo2 = MAX30102_Calc_SpO2(red_buf,
                                  ir_buf,
                                  len,
                                  &result->spo2);

    if (ret_spo2 == 0)
    {
        result->spo2_valid = 1;
    }

    if ((result->heart_valid == 0) && (result->spo2_valid == 0))
    {
        return 1;
    }

    return 0;
}