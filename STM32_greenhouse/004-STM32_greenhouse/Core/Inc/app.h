#ifndef APP_H
#define APP_H

#include "main.h"
#include "cmsis_os.h"

/* 手动模式保持时间：最后一次手动指令后 60s 无新指令即回到自动 */
#define MANUAL_HOLD_MS   60000u

/* I2C1 总线互斥量，实体在 freertos.c */
extern osMutexId g_i2c_mutex;

/* 上电初始化：调度器启动前调用（OLED / DHT11 / BH1750 / EEPROM 阈值） */
void APP_InitLocal(void);

/* 按键处理：一次只处理一个按键，idx = 0~5 */
void APP_Key(uint8_t idx);

/* 自动控制：阈值 + 回差，仅自动模式改执行器与报警位 */
void APP_Data_Ctrl(void);

/* 手动模式计时 */
void APP_ManualTouch(void);
void APP_ManualPoll(void);

/* 传感器采样：DHT11 + BH1750 + 土壤 ADC */
void APP_GET_DATA(void);

/* 阈值存取 */
void APP_STORE(void);
void APP_Read(void);

/* I2C1 总线锁：OLED / BH1750 / EEPROM 三处共用 */
void APP_I2C_Lock(void);
void APP_I2C_Unlock(void);

#endif //APP_H
