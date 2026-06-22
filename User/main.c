#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/SDRAM/sdram.h"

#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./BSP/MPU/mpu.h"
#include "./BSP/LCD/ltdc.h"
#include "./MALLOC/malloc.h"
#include "lvgl_demo.h"
#include "./SYSTEM/usart2/usart2.h"
#include "stdio.h" 
#include "./BSP/BUZZER/buzzer.h"

const char *SRAM_NAME_BUF[SRAMBANK] = {" SRAMIN ", " SRAMEX ", " SRAM12 ", " SRAM4  ", "SRAMDTCM", "SRAMITCM"};

int main(void)
{
    
    /* ================= 1. 底层硬件初始化 ================= */
    HAL_Init();                              /* 初始化HAL库 */
    sys_stm32_clock_init(160, 5, 2, 4);      /* 设置时钟, 400Mhz */
    delay_init(400);                         /* 延时初始化 */
    usart_init(115200);                      /* 串口1初始化-->打印调试 */
    usart2_init(115200);                     /* 串口2初始化-->ESP8266 */
   
    mpu_memory_protection();                 /* MPU 保护配置 (H7 必须，防止非法内存访问) */
    
    /* ================= 2. 外设驱动初始化 ================= */
    led_init();                              /* 初始化LED */
    sdram_init();                            /* 初始化SDRAM */
    lcd_init();                              /* 初始化LCD */
    key_init();                              /* 初始化KEY */ 
    buzzer_init();

    /* ================= 3. 内存池管理初始化 ================= */
    my_mem_init(SRAMIN);                     /* 初始化内部内存池(AXI) */
    my_mem_init(SRAMEX);                     /* 初始化外部内存池(SDRAM) */
    my_mem_init(SRAM12);                     /* 初始化SRAM12内存池(SRAM1+SRAM2) */
    my_mem_init(SRAM4);                      /* 初始化SRAM4内存池(SRAM4) */
    my_mem_init(SRAMDTCM);                   /* 初始化DTCM内存池(DTCM) */
    my_mem_init(SRAMITCM);                   /* 初始化ITCM内存池(ITCM) */
    
    /* ================= 4. 启动 FreeRTOS ================= */
    lvgl_demo();
}





