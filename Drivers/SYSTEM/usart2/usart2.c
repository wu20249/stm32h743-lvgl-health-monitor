#include "./SYSTEM/usart2/usart2.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"

UART_HandleTypeDef g_uart2_handle;
uint8_t rx_temp;

char esp8266_rx_buf[ESP8266_RX_BUF_SIZE];
volatile uint16_t esp8266_rx_len = 0;

void esp8266_rx_clear(void)
{
    memset(esp8266_rx_buf, 0, ESP8266_RX_BUF_SIZE);
    esp8266_rx_len = 0;
}

void usart2_rx_callback(void)
{
    if (esp8266_rx_len < ESP8266_RX_BUF_SIZE - 1)
    {
        esp8266_rx_buf[esp8266_rx_len++] = rx_temp;
        esp8266_rx_buf[esp8266_rx_len] = '\0';
    }

    HAL_UART_Receive_IT(&g_uart2_handle, &rx_temp, 1);
}

/* USART2 初始化 */
void usart2_init(uint32_t baudrate)
{
    g_uart2_handle.Instance = USART2;

    g_uart2_handle.Init.BaudRate = baudrate;
    g_uart2_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart2_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart2_handle.Init.Parity = UART_PARITY_NONE;
    g_uart2_handle.Init.Mode = UART_MODE_TX_RX;
    g_uart2_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart2_handle.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&g_uart2_handle);

    HAL_UART_Receive_IT(&g_uart2_handle, &rx_temp, 1);
}

/* 发送AT命令，自动补 \r\n */
void esp_send(const char *cmd)
{
    HAL_UART_Transmit(&g_uart2_handle, (uint8_t *)cmd, strlen(cmd), 1000);

    {
        uint8_t end[2] = {0x0D, 0x0A};
        HAL_UART_Transmit(&g_uart2_handle, end, 2, 1000);
    }
}

/* 原样发送字符串，不补 \r\n */
void esp_send_string(const char *str)
{
    HAL_UART_Transmit(&g_uart2_handle, (uint8_t *)str, strlen(str), 1000);
}


/* USART2 中断 */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&g_uart2_handle);
}

/* 串口2 printf */
void u2_printf(char *fmt, ...)
{
    uint8_t buf[256];
    uint16_t len;

    va_list ap;
    va_start(ap, fmt);
    vsprintf((char *)buf, fmt, ap);
    va_end(ap);

    len = strlen((const char *)buf);
    HAL_UART_Transmit(&g_uart2_handle, buf, len, 1000);
}