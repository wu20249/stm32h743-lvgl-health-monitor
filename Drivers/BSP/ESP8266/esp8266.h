#ifndef __ESP8266_H
#define __ESP8266_H

void ESP8266_Init(void);
int ESP8266_Send_Temp(const char *identifier, const char *value_json);

#endif