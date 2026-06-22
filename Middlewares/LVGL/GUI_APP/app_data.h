#ifndef __APP_DATA_H
#define __APP_DATA_H

#include <stdint.h>

/* ×ËÌ¬×´Ì¬ */
#define POSTURE_NORMAL  0
#define POSTURE_FALL    1

/* DHT11 */
extern volatile uint8_t g_temperature;
extern volatile uint8_t g_humidity;
extern volatile uint8_t g_dht11_ok;

/* MAX30102 */
extern volatile uint8_t g_heart_rate;
extern volatile uint8_t g_spo2;
extern volatile uint8_t g_max30102_ok;

/* MPU6050 */
extern volatile uint8_t g_posture_status;

/* WiFi / MQTT / ÉÏ´«×´Ì¬ */
extern volatile uint8_t g_wifi_ok;
extern volatile uint8_t g_mqtt_ok;
extern volatile uint8_t g_upload_ok;

#endif