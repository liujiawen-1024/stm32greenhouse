#ifndef ESP_01S_H
#define ESP_01S_H

#include "main.h"

#define WIFI_SSID       "Aa123456"
#define WIFI_PASS       "Aa123456"
#define PRODUCT_KEY     "OxvNPUbCbM"
#define DEVICE_NAME     "F103A1001"
#define MQTT_TOKEN      "version=2018-10-31&res=products%2FOxvNPUbCbM%2Fdevices%2FF103A1001&et=1950399069&method=md5&sign=o50QJsTFGnpXNRG5fppJVA%3D%3D"

/******WIFI 配置******/
#define ESP01S_WIFI     "AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASS "\"\r\n"
/******MQTT AT 指令******/
#define MQTT_USERCFG    "AT+MQTTUSERCFG=0,1,\"" DEVICE_NAME "\",\"" PRODUCT_KEY "\",\"" MQTT_TOKEN "\",0,0,\"\"\r\n"
#define MQTT_CONN       "AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1\r\n"
#define MQTT_SUB_SET    "AT+MQTTSUB=0,\"$sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/property/set\",0\r\n"
/******函数发送******/
#define MQTT_PUB_HEAD \
    "AT+MQTTPUB=0,\"$sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/property/post\",\"{\\\"id\\\":\\\"123\\\"\\,\\\"params\\\":{"
#define MQTT_PUB_TAIL   "}}\",0,0\r\n"
/******函数声明******/
uint8_t ESP01S_Init(void);
void ESP01S_Send_Data(void);
void ESP01S_Send_TH(void);
void ESP01S_Send_SW(void);
#endif //ESP_01S_H
