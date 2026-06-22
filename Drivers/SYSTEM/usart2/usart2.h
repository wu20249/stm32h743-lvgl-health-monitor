#ifndef __USART2_H
#define __USART2_H

#include "./SYSTEM/sys/sys.h"

#define ESP8266_RX_BUF_SIZE 1024

extern UART_HandleTypeDef g_uart2_handle;
extern uint8_t rx_temp;

extern char esp8266_rx_buf[ESP8266_RX_BUF_SIZE];
extern volatile uint16_t esp8266_rx_len;

void usart2_init(uint32_t baudrate);
void esp_send(const char *cmd);
void esp_send_string(const char *str);
void u2_printf(char *fmt, ...);

void esp8266_rx_clear(void);
void usart2_rx_callback(void);

#endif