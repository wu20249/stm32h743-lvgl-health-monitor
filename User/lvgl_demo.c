#include "lvgl_demo.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/DHT11/dht11.h"  
/*FreeRTOS*********************************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "./BSP/ESP8266/esp8266.h"
#include "string.h"
#include "stdio.h"
#include "./BSP/MPU6050/mpu6050.h"
#include "./BSP/MAX30102/max30102.h"
#include "health_ui.h"
#include "app_data.h"
#include "lv_port_disp_template.h"
#include "lv_port_indev_template.h"
#include "./BSP/BUZZER/buzzer.h"

/******************************************************************************************************/

#define START_TASK_PRIO 1                   /* 任务优先级 */
#define START_STK_SIZE  128                 /* 任务堆栈大小 */
TaskHandle_t            StartTask_Handler;  /* 任务句柄 */
void start_task(void *pvParameters);        /* 任务函数 */

/* DHT11 任务配置 */
/* 注意：堆栈大小建议给大一点，因为涉及 LCD 字符串处理和浮点/整形转换 */
#define DHT11_TASK_PRIO     4
#define DHT11_TASK_STK_SIZE 256 
TaskHandle_t DHT11Task_Handler;
/* DHT11 心率血氧全局变量 */
void task_dht11(void *pvParameters);
volatile uint8_t g_temperature = 0;
volatile uint8_t g_humidity = 0;
volatile uint8_t g_dht11_ok = 0;

/* ESP8266 任务配置 */
#define WIFI_TASK_PRIO      5
#define WIFI_TASK_STK_SIZE  512
TaskHandle_t WifiTask_Handler;
void task_wifi(void *pvParameters);

/* MPU6050 任务配置 */
#define MPU6050_TASK_PRIO      4
#define MPU6050_TASK_STK_SIZE  512
TaskHandle_t MPU6050Task_Handler;
void task_mpu6050(void *pvParameters);
/* 姿态状态 */
//#define POSTURE_NORMAL  0
//#define POSTURE_FALL    1
volatile uint8_t g_posture_status = POSTURE_NORMAL;
/* 跌倒检测参数 */
#define FALL_ACC_IMPACT_TH       2.5f    /* 冲击加速度阈值，单位 g */
#define FALL_GYRO_IMPACT_TH      250.0f  /* 冲击角速度阈值，单位 °/s */

#define FALL_STILL_ACC_LOW       0.75f   /* 静止时加速度模长下限 */
#define FALL_STILL_ACC_HIGH      1.25f   /* 静止时加速度模长上限 */
#define FALL_STILL_GYRO_TH       40.0f   /* 静止时角速度阈值 */

#define FALL_STILL_COUNT_TH      10      /* 连续静止次数，10次约等于1秒 */
#define FALL_CHECK_WINDOW_MS     3000    /* 冲击后3秒内检查是否静止 */
#define FALL_HOLD_TIME_MS        10000   /* 摔倒状态保持10秒，方便演示 */
static void Fall_Detect_Update(MPU6050_Data_t *mpu);

/* MAX30102 任务配置 */
#define MAX30102_TASK_PRIO      4
#define MAX30102_TASK_STK_SIZE  256
TaskHandle_t MAX30102Task_Handler;
void task_max30102(void *pvParameters);
/* MAX30102 心率血氧全局变量 */
volatile uint8_t g_heart_rate = 0;
volatile uint8_t g_spo2 = 0;
volatile uint8_t g_max30102_ok = 0;

/* LVGL 任务配置 */
#define LVGL_TASK_PRIO      3
#define LVGL_TASK_STK_SIZE  1024
TaskHandle_t LVGLTask_Handler;
void task_lvgl(void *pvParameters);

/* WiFi / MQTT / 上传状态 */
volatile uint8_t g_wifi_ok = 0;
volatile uint8_t g_mqtt_ok = 0;
volatile uint8_t g_upload_ok = 0;

/******************************************************************************************************/

/* LCD刷屏时使用的颜色 */
uint16_t lcd_discolor[11] = {WHITE, BLACK, BLUE, RED,
                             MAGENTA, GREEN, CYAN, YELLOW,
                             BROWN, BRRED, GRAY};

/**
 * @brief       FreeRTOS例程入口函数
 * @param       无
 * @retval      无
 */
void lvgl_demo(void)
{
    /*
     * LVGL 初始化
     * 注意：main.c 里不用初始化，放在这里
     */
    lv_init();

    /*
     * 正点原子 LVGL 显示接口初始化
     */
    lv_port_disp_init();

    /*
     * 正点原子 LVGL 输入设备初始化
     * 如果暂时不用触摸，也可以先保留
     */
    lv_port_indev_init();

    /*
     * 创建开始任务
     */
    xTaskCreate((TaskFunction_t )start_task,
                (const char*    )"start_task",
                (uint16_t       )START_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )START_TASK_PRIO,
                (TaskHandle_t*  )&StartTask_Handler);

    /*
     * 开启 FreeRTOS 调度
     */
    vTaskStartScheduler();
}

/**
 * @brief       start_task
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void start_task(void *pvParameters)
{
    BaseType_t max_ret;
    taskENTER_CRITICAL();           /* 进入临界区 */
                
    /* DHT11 任务 */
    xTaskCreate((TaskFunction_t )task_dht11,
                (const char*    )"task_dht11",
                (uint16_t       )DHT11_TASK_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )DHT11_TASK_PRIO,
                (TaskHandle_t*  )&DHT11Task_Handler);
                
    /* WIFI任务 */
    xTaskCreate((TaskFunction_t )task_wifi,
            (const char*    )"task_wifi",
            (uint16_t       )WIFI_TASK_STK_SIZE,
            (void*          )NULL,
            (UBaseType_t    )WIFI_TASK_PRIO,
            (TaskHandle_t*  )&WifiTask_Handler);
    
     /* mpu6050任务 */
     xTaskCreate((TaskFunction_t )task_mpu6050,
            (const char*    )"task_mpu6050",
            (uint16_t       )MPU6050_TASK_STK_SIZE,
            (void*          )NULL,
            (UBaseType_t    )MPU6050_TASK_PRIO,
            (TaskHandle_t*  )&MPU6050Task_Handler);
            
     /* MAX30102 任务 */
    max_ret = xTaskCreate((TaskFunction_t )task_max30102,
                      (const char*    )"task_max30102",
                      (uint16_t       )MAX30102_TASK_STK_SIZE,
                      (void*          )NULL,
                      (UBaseType_t    )MAX30102_TASK_PRIO,
                      (TaskHandle_t*  )&MAX30102Task_Handler);
           
    /* LVGL 任务 */
    xTaskCreate((TaskFunction_t )task_lvgl,
            (const char*    )"task_lvgl",
            (uint16_t       )LVGL_TASK_STK_SIZE,
            (void*          )NULL,
            (UBaseType_t    )LVGL_TASK_PRIO,
            (TaskHandle_t*  )&LVGLTask_Handler);
                               
    vTaskDelete(StartTask_Handler); /* 删除开始任务 */
    taskEXIT_CRITICAL();            /* 退出临界区 */
}

/**
 * @brief  DHT11 温湿度读取任务
 */
void task_dht11(void *pvParameters)
{
    uint8_t temperature;
    uint8_t humidity;
    uint8_t res;

    /*
     * 初始化 DHT11。
     * 不再直接操作 LCD，只更新全局状态变量。
     */
    while (dht11_init())
    {
        g_dht11_ok = 0;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    g_dht11_ok = 1;

    while (1)
    {
        res = dht11_read_data(&temperature, &humidity);

        if (res == 0)
        {
            g_temperature = temperature;
            g_humidity = humidity;
            g_dht11_ok = 1;
        }
        else
        {
            g_dht11_ok = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


/**
 * @brief  ESP8266 WiFi / MQTT 上传任务
 */
void task_wifi(void *pvParameters)
{
    char value_buf[24];
    uint8_t upload_hr;
    uint8_t upload_spo2;
    const char *posture_value;

    uint8_t send_ok;
    uint8_t round_ok;
    uint32_t upload_count = 0;

    /*
     * 初始状态
     */
    g_wifi_ok = 0;
    g_mqtt_ok = 0;
    g_upload_ok = 0;

    vTaskDelay(pdMS_TO_TICKS(3000));   /* 等系统稳定、ESP8266上电稳定 */

    /*
     * ESP8266 初始化
     * 注意：如果你的 ESP8266_Init() 是 void 类型，
     * 这里暂时认为执行完就是 WiFi 和 MQTT 已连接。
     */
    ESP8266_Init();

    g_wifi_ok = 1;
    g_mqtt_ok = 1;

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1)
    {
        /*
         * 本轮上传状态
         * 只有五条属性都发送成功，round_ok 才保持为 1
         */
        round_ok = 1;

        /*
         * 1. 上传温度 temperature
         */
        snprintf(value_buf, sizeof(value_buf), "%d", g_temperature);

        send_ok = ESP8266_Send_Temp("temperature", value_buf);

        if (send_ok == 0)
        {
            round_ok = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(300));

        /*
         * 2. 上传湿度 humidity
         */
        snprintf(value_buf, sizeof(value_buf), "%d", g_humidity);

        send_ok = ESP8266_Send_Temp("humidity", value_buf);

        if (send_ok == 0)
        {
            round_ok = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(300));

        /*
         * 3. 上传心率 heart_rate
         */
        if (g_max30102_ok)
        {
            upload_hr = g_heart_rate;
            upload_spo2 = g_spo2;
        }
        else
        {
            upload_hr = 0;
            upload_spo2 = 0;
        }

        snprintf(value_buf, sizeof(value_buf), "%d", upload_hr);

        send_ok = ESP8266_Send_Temp("heart_rate", value_buf);

        if (send_ok == 0)
        {
            round_ok = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(300));

        /*
         * 4. 上传血氧 spo2
         */
        snprintf(value_buf, sizeof(value_buf), "%d", upload_spo2);

        send_ok = ESP8266_Send_Temp("spo2", value_buf);

        if (send_ok == 0)
        {
            round_ok = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(300));

        /*
         * 5. 上传姿态状态 posture_status
         *
         * posture_status 是 string 类型，
         * 所以 value_json 本身要带双引号。
         */
        if (g_posture_status == POSTURE_FALL)
        {
            posture_value = "\"\\u6454\\u5012\"";     /* "摔倒" */
            BUZZER_ON();
        }
        else
        {
            posture_value = "\"\\u6b63\\u5e38\"";     /* "正常" */
            BUZZER_OFF();
        }

        send_ok = ESP8266_Send_Temp("posture_status", posture_value);

        if (send_ok == 0)
        {
            round_ok = 0;
        }

        /*
         * 根据本轮上传结果，更新 LVGL 界面状态
         */
        if (round_ok)
        {
            g_upload_ok = 1;
            g_mqtt_ok = 1;
        }
        else
        {
            g_upload_ok = 0;

            /*
             * 如果连续上传失败，大概率 MQTT 或 ESP8266 出问题。
             * 这里先简单标记 MQTT 异常。
             */
            g_mqtt_ok = 0;
        }

        /*
         * 串口打印上传轮次
         * 这是串口输出，不会影响 LVGL 屏幕。
         */
        printf("Upload cycle:%lu, result:%d\r\n",
               (unsigned long)upload_count++,
               round_ok);

        /*
         * 每隔 3 秒上传一轮
         */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}


/**
 * @brief  跌倒检测状态更新
 */
static void Fall_Detect_Update(MPU6050_Data_t *mpu)
{
    float acc_sq;
    float gyro_sq;

    TickType_t now_tick;

    static uint8_t fall_candidate = 0;
    static uint8_t still_count = 0;
    static uint8_t fall_latched = 0;

    static TickType_t candidate_tick = 0;
    static TickType_t fall_tick = 0;

    if (mpu == NULL)
    {
        return;
    }

    now_tick = xTaskGetTickCount();

    /*
     * 加速度模长平方：
     * acc = sqrt(ax^2 + ay^2 + az^2)
     * 为了减少运算，这里不求 sqrt，直接比较平方
     */
    acc_sq = mpu->acc_x_g * mpu->acc_x_g +
             mpu->acc_y_g * mpu->acc_y_g +
             mpu->acc_z_g * mpu->acc_z_g;

    /*
     * 角速度模长平方：
     * gyro = sqrt(gx^2 + gy^2 + gz^2)
     */
    gyro_sq = mpu->gyro_x_dps * mpu->gyro_x_dps +
              mpu->gyro_y_dps * mpu->gyro_y_dps +
              mpu->gyro_z_dps * mpu->gyro_z_dps;

    /*
     * 如果已经确认摔倒，保持一段时间，
     * 方便 LVGL 和云端看到“摔倒”状态。
     */
    if (fall_latched)
    {
        g_posture_status = POSTURE_FALL;

        /*
         * 演示用：10秒后自动恢复正常。
         * 如果你想报警一直保持，就删除这个 if。
         */
        if ((now_tick - fall_tick) > pdMS_TO_TICKS(FALL_HOLD_TIME_MS))
        {
            fall_latched = 0;
            fall_candidate = 0;
            still_count = 0;
            g_posture_status = POSTURE_NORMAL;
        }

        return;
    }

    /*
     * 默认状态为正常
     */
    g_posture_status = POSTURE_NORMAL;

    /*
     * 第一步：检测冲击
     * 加速度突然变大，或者角速度突然变大，都认为可能发生跌倒。
     */
    if ((acc_sq > (FALL_ACC_IMPACT_TH * FALL_ACC_IMPACT_TH)) ||
        (gyro_sq > (FALL_GYRO_IMPACT_TH * FALL_GYRO_IMPACT_TH)))
    {
        fall_candidate = 1;
        candidate_tick = now_tick;
        still_count = 0;
    }

    /*
     * 第二步：如果出现过冲击，则在一定时间内检查是否进入静止状态。
     */
    if (fall_candidate)
    {
        /*
         * 静止条件：
         * 1. 加速度模长接近 1g
         * 2. 角速度很小
         */
        if ((acc_sq > (FALL_STILL_ACC_LOW * FALL_STILL_ACC_LOW)) &&
            (acc_sq < (FALL_STILL_ACC_HIGH * FALL_STILL_ACC_HIGH)) &&
            (gyro_sq < (FALL_STILL_GYRO_TH * FALL_STILL_GYRO_TH)))
        {
            still_count++;
        }
        else
        {
            if (still_count > 0)
            {
                still_count--;
            }
        }

        /*
         * 连续静止约 1 秒，确认摔倒。
         */
        if (still_count >= FALL_STILL_COUNT_TH)
        {
            fall_latched = 1;
            fall_tick = now_tick;

            fall_candidate = 0;
            still_count = 0;

            g_posture_status = POSTURE_FALL;
        }

        /*
         * 如果冲击后 3 秒内没有进入静止状态，取消本次疑似跌倒。
         */
        if ((now_tick - candidate_tick) > pdMS_TO_TICKS(FALL_CHECK_WINDOW_MS))
        {
            fall_candidate = 0;
            still_count = 0;
        }
    }
}


/**
 * @brief  MPU6050 姿态检测任务
 */
void task_mpu6050(void *pvParameters)
{
    MPU6050_Data_t mpu;
    uint8_t init_ret;

    /*
     * 初始化 MPU6050。
     * 不再直接操作 LCD，只更新 g_posture_status。
     */
    while (1)
    {
        init_ret = MPU6050_Init();

        if (init_ret == 0)
        {
            break;
        }

        /*
         * 初始化失败时保持正常状态，继续重试。
         */
        g_posture_status = POSTURE_NORMAL;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (1)
    {
        if (MPU6050_Read_Data(&mpu) == 0)
        {
            /*
             * 根据 MPU6050 数据更新姿态状态：
             * 正常 / 摔倒
             */
            Fall_Detect_Update(&mpu);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


/**
 * @brief  MAX30102 心率血氧检测任务
 */
void task_max30102(void *pvParameters)
{
    static uint32_t red_buf[MAX30102_SAMPLE_COUNT];
    static uint32_t ir_buf[MAX30102_SAMPLE_COUNT];

    MAX30102_Data_t max_data;
    MAX30102_Result_t result;

    uint16_t sample_cnt = 0;
    uint8_t ret;

    /*
     * 初始化 MAX30102。
     * 不再直接操作 LCD，只更新：
     * g_heart_rate
     * g_spo2
     * g_max30102_ok
     */
    ret = MAX30102_Init();

    if (ret != 0)
    {
        g_max30102_ok = 0;
    }

    while (1)
    {
        /*
         * 如果初始化失败，则周期性重试。
         */
        if (ret != 0)
        {
            g_max30102_ok = 0;

            vTaskDelay(pdMS_TO_TICKS(1000));

            ret = MAX30102_Init();

            continue;
        }

        if (MAX30102_Read_FIFO(&max_data) == 0)
        {
            if (MAX30102_Check_Finger(max_data.ir) == 0)
            {
                /*
                 * 未检测到手指
                 */
                sample_cnt = 0;
                g_max30102_ok = 0;
            }
            else
            {
                /*
                 * 采样缓存
                 */
                red_buf[sample_cnt] = max_data.red;
                ir_buf[sample_cnt] = max_data.ir;
                sample_cnt++;

                if (sample_cnt >= MAX30102_SAMPLE_COUNT)
                {
                    if (MAX30102_Calc_HR_SpO2(red_buf,
                                              ir_buf,
                                              MAX30102_SAMPLE_COUNT,
                                              MAX30102_SAMPLE_RATE_HZ,
                                              &result) == 0)
                    {
                        if (result.heart_valid && result.spo2_valid)
                        {
                            g_heart_rate = result.heart_rate;
                            g_spo2 = result.spo2;
                            g_max30102_ok = 1;
                        }
                        else
                        {
                            g_max30102_ok = 0;
                        }
                    }
                    else
                    {
                        g_max30102_ok = 0;
                    }

                    sample_cnt = 0;
                }
            }
        }
        else
        {
            /*
             * FIFO 读取失败
             */
            g_max30102_ok = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void task_lvgl(void *pvParameters)
{
    /*
     * 创建 LVGL 主界面
     */
    health_ui_create();

    while (1)
    {
        /*
         * LVGL 必须周期调用
         */
        lv_timer_handler();

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}