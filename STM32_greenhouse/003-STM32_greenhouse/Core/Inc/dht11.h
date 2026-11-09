#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

#define DHT11_POWER_ON_WAIT_MS      1000    /* DHT11上电稳定 */
#define DHT11_START_LOW_MS          20      /* 主机起始信号 */

#define DHT11_RESPONSE_TIMEOUT_US   120     /* 等待 DHT11 响应 */
#define DHT11_BIT_TIMEOUT_US        100     /* 读取单个数据位 */

#define DHT11_DQ_OUT(x)    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, \
                                            ((x) ? GPIO_PIN_SET : GPIO_PIN_RESET))
#define DHT11_DQ_IN        HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin)


/* 方向切换 */
void DHT11_IoOut(void);
void DHT11_IoIn(void);
#define DHT11_IO_OUT()     DHT11_IoOut()
#define DHT11_IO_IN()      DHT11_IoIn()

uint8_t DHT11_Init(void);
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);
uint8_t DHT11_Read_Raw(uint8_t *buf);   /* buf[5]：湿度整数/小数、温度整数/小数、校验和 */
uint8_t DHT11_Read_Byte(void);
uint8_t DHT11_Read_Bit(void);
uint8_t DHT11_Check(void);
void    DHT11_Rst(void);

#endif
