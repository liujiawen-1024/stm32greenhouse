#ifndef __BH1750_H
#define __BH1750_H

#include "main.h"

extern I2C_HandleTypeDef hi2c1;
#define BH1750_I2C_HANDLE         hi2c1

/* 单次 I2C 传输超时(ms) */
#define BH1750_I2C_TIMEOUT        100U

/* 一次测量需要的等待时间(ms)：H 分辨率 120(典型)/180(最大)，L 分辨率更慢 */
#define BH1750_MEASURE_TIME_MS    180U

/* 满量程光照(lx)，用于 LIght_Intensity() 的百分比换算 */
#define BH1750_FULL_SCALE_LUX     3000U

#define BH1750_ADDR_7BIT          0x23U
#define BH1750_I2C_ADDR           0x46

/* BH1750 指令码 */
#define POWER_OFF                 0x00
#define POWER_ON                  0x01
#define MODULE_RESET              0x07
#define CONTINUE_H_MODE           0x10
#define CONTINUE_H_MODE2          0x11
#define CONTINUE_L_MODE           0x13
#define ONE_TIME_H_MODE           0x20
#define ONE_TIME_H_MODE2          0x21
#define ONE_TIME_L_MODE           0x23

/* 测量模式
   使用 CONTINUE_xx 连续模式时，BH1750_Read_Measure() 可随时读取最新数据；
   使用 ONE_TIME_xx 单次模式时，每次读取前都要调用 BH1750_Start_Measure()，
   等转换完成后再读取。 */
#define Measure_Mode              CONTINUE_H_MODE

/* 分辨率：光照强度(lx) = (High Byte + Low Byte) / 1.2 * 测量精度 */
#if ((Measure_Mode == CONTINUE_H_MODE2) || (Measure_Mode == ONE_TIME_H_MODE2))
	#define Resolurtion           0.5f
#elif ((Measure_Mode == CONTINUE_H_MODE) || (Measure_Mode == ONE_TIME_H_MODE))
	#define Resolurtion           1.0f
#elif ((Measure_Mode == CONTINUE_L_MODE) || (Measure_Mode == ONE_TIME_L_MODE))
	#define Resolurtion           4.0f
#endif

/* 应用层函数，返回 0 表示成功，非 0 表示失败 */
uint8_t  BH1750_Init(void);                 /* 上电并配置测量模式 */
uint8_t  BH1750_IsReady(void);              /* 检测器件是否应答：0 在线，非 0 未探测到 */
uint8_t  BH1750_Byte_Write(uint8_t data);   /* 写一个命令字节 */
uint8_t  BH1750_Read_Raw(uint16_t *raw);    /* 读取 16bit 原始数据 */
uint16_t BH1750_Read_Measure(void);         /* 返回原始测量值，失败返回 0 */
float    BH1750_Read_Lux(void);             /* 返回光照强度，单位 lx */
void     BH1750_Start_Measure(void);        /* 单次模式下：触发一次测量并等待完成 */
void     BH1750_Power_ON(void);
void     BH1750_Power_OFF(void);
void     BH1750_RESET(void);

void Light_Intensity(uint8_t *ill_cal);

#endif
