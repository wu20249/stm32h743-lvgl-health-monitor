#include "./BSP/ESP8266/esp8266.h"
#include "./SYSTEM/usart2/usart2.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"

#define WIFI_SSID "HONOR 50"
#define WIFI_PASS "1234567890"

#define MQTT_USERCFG  "AT+MQTTUSERCFG=0,1,\"NULL\",\"Senior_citizen_monitoring_system&a1dOP7c0d1P\",\"d609617c83ec7766313295dff8a6a8f5e12914f9c902b752eac1269183993c13\",0,0,\"\""
#define MQTT_CLIENTID "AT+MQTTCLIENTID=0,\"a1dOP7c0d1P.Senior_citizen_monitoring_system|securemode=2\\,signmethod=hmacsha256\\,timestamp=1773468416118|\""
#define MQTT_CONN "AT+MQTTCONN=0,\"a1dOP7c0d1P.iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883,1"
#define MQTT_TOPIC "/sys/a1dOP7c0d1P/Senior_citizen_monitoring_system/thing/event/property/post"

static int ESP8266_SendCmd_Check(char *cmd, char *expect, uint32_t timeout_ms);
static int ESP8266_SendCmd_Check(char *cmd, char *expect, uint32_t timeout_ms)
{
    TickType_t start_tick;

    esp8266_rx_clear();

    esp_send(cmd);

    start_tick = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(timeout_ms))
    {
        if (strstr(esp8266_rx_buf, "ERROR") != NULL)
        {
            return -1;
        }

        if (strstr(esp8266_rx_buf, "FAIL") != NULL)
        {
            return -2;
        }

        if (expect != NULL && strstr(esp8266_rx_buf, expect) != NULL)
        {
            return 1;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return 0;
}

void ESP8266_Init(void)
{
    char wifi_cmd[120];

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP8266_SendCmd_Check("AT", "OK", 2000);

    ESP8266_SendCmd_Check("AT+CWMODE=1", "OK", 2000);

    sprintf(wifi_cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASS);
    ESP8266_SendCmd_Check(wifi_cmd, "WIFI GOT IP", 15000);

    ESP8266_SendCmd_Check(MQTT_USERCFG, "OK", 3000);

    ESP8266_SendCmd_Check(MQTT_CLIENTID, "OK", 3000);

    ESP8266_SendCmd_Check(MQTT_CONN, "+MQTTCONNECTED", 10000);

    ESP8266_SendCmd_Check("AT+MQTTCONN?", "OK", 3000);

}

static int ESP8266_WaitFor(const char *target, uint32_t timeout_ms)
{
    TickType_t start_tick;

    if (target == NULL)
    {
        return 0;
    }

    start_tick = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(timeout_ms))
    {
        if (strstr(esp8266_rx_buf, target) != NULL)
        {
            return 1;
        }

        if (strstr(esp8266_rx_buf, "ERROR") != NULL)
        {
            return 0;
        }

        if (strstr(esp8266_rx_buf, "FAIL") != NULL)
        {
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return 0;
}

int ESP8266_Send_Temp(const char *identifier, const char *value_json)
{
    char payload[220];
    char cmd[280];
    int payload_len;
    static uint32_t msg_id = 1;

    if ((identifier == NULL) || (value_json == NULL))
    {
        return 0;
    }

    snprintf(payload,
             sizeof(payload),
             "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"%s\":%s},\"method\":\"thing.event.property.post\"}",
             msg_id++,
             identifier,
             value_json);

    payload_len = strlen(payload);

    snprintf(cmd,
             sizeof(cmd),
             "AT+MQTTPUBRAW=0,\"%s\",%d,1,0",
             MQTT_TOPIC,
             payload_len);

    esp8266_rx_clear();

    esp_send(cmd);

    if (ESP8266_WaitFor(">", 3000) == 0)
    {
        return 0;
    }

    esp8266_rx_clear();

    /*
     * RAW 模式下，payload 不要自动补 \r\n
     */
    esp_send_string(payload);

    if (ESP8266_WaitFor("+MQTTPUB:OK", 5000) == 0)
    {
        return 0;
    }

    return 1;
}
