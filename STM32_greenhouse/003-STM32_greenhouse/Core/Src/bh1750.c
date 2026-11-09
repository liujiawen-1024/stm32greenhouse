#include "bh1750.h"

#include <stdio.h>

static void BH1750_I2C_Recover(void)
{
	HAL_I2C_DeInit(&BH1750_I2C_HANDLE);
	HAL_I2C_Init(&BH1750_I2C_HANDLE);
}

uint8_t BH1750_IsReady(void)
{
	if (HAL_I2C_IsDeviceReady(&BH1750_I2C_HANDLE, BH1750_I2C_ADDR, 3, BH1750_I2C_TIMEOUT) == HAL_OK)
		return 0;
	else
		return 1;
}

uint8_t BH1750_Byte_Write(uint8_t data)
{
	if (HAL_I2C_Master_Transmit(&BH1750_I2C_HANDLE, BH1750_I2C_ADDR, &data, 1, BH1750_I2C_TIMEOUT) != HAL_OK)
	{
		BH1750_I2C_Recover();
		return 1;
	}
	return 0;
}

uint8_t BH1750_Read_Raw(uint16_t *raw)
{
	uint8_t buf[2] = {0, 0};

	if (raw == 0)
		return 1;

	if (HAL_I2C_Master_Receive(&BH1750_I2C_HANDLE, BH1750_I2C_ADDR, buf, 2, BH1750_I2C_TIMEOUT) != HAL_OK)
	{
		BH1750_I2C_Recover();
		return 2;
	}

	*raw = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
	return 0;
}

uint16_t BH1750_Read_Measure(void)
{
	uint16_t receive_data = 0;

	if (BH1750_Read_Raw(&receive_data) != 0)
		return 0;

	return receive_data;
}

float BH1750_Read_Lux(void)
{
	uint16_t raw = 0;

	if (BH1750_Read_Raw(&raw) != 0)
		return 0.0f;

	return (float)raw / 1.2f * Resolurtion;
}

void BH1750_Power_ON(void)
{
	BH1750_Byte_Write(POWER_ON);
}

void BH1750_Power_OFF(void)
{
	BH1750_Byte_Write(POWER_OFF);
}

void BH1750_RESET(void)
{
	BH1750_Byte_Write(MODULE_RESET);
}

void BH1750_Start_Measure(void)
{
	BH1750_Byte_Write(Measure_Mode);
	HAL_Delay(BH1750_MEASURE_TIME_MS);      // 等待转换完成
}

uint8_t BH1750_Init(void)
{
	HAL_Delay(10);                          // 等待 BH1750 上电稳定

	if (BH1750_Byte_Write(POWER_ON) != 0)   // BH1750 上电
		return 1;
	HAL_Delay(10);                          // 等待内部振荡器稳定

	if (BH1750_Byte_Write(Measure_Mode) != 0)   // 设置测量模式
		return 3;

	HAL_Delay(BH1750_MEASURE_TIME_MS);      // 等待第一次转换完成
	return 0;
}

void Light_Intensity(uint8_t *ill_cal)
{
	if (ill_cal == NULL) return;

	uint32_t Light_Val = (uint32_t)BH1750_Read_Lux() * 100U / BH1750_FULL_SCALE_LUX;

	*ill_cal = (Light_Val >= 100U) ? 100U : (uint8_t)Light_Val;
}
