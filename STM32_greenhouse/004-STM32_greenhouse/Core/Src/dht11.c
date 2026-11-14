#include "dht11.h"

static uint8_t s_dht11_timeout = 0;		// 超时标记

static void DHT11_DWT_Init(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* 使能跟踪单元 */
	DWT->CYCCNT = 0U;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;             /* 使能周期计数器 */
}

static void dht11_delay_us(uint32_t us)
{
	uint32_t start;
	uint32_t ticks;

	/* 万一没调用过 DHT11_Init()，这里补一次，避免 CYCCNT 不走的死循环 */
	if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
	{
		DHT11_DWT_Init();
	}

	start = DWT->CYCCNT;
	ticks = us * (SystemCoreClock / 1000000U);

	/* CYCCNT 是 32 位，减法运算天然支持回绕，只要 ticks < 2^32 即可 */
	while ((DWT->CYCCNT - start) < ticks)
	{
	}
}

#define DHT11_DELAY_US(us)     dht11_delay_us((uint32_t)(us))


void DHT11_IoOut(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin   = DHT11_Pin;
	GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;          /* 输出模式下 Pull 字段无效 */
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

void DHT11_IoIn(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin   = DHT11_Pin;
	GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull  = GPIO_PULLUP;          /* 内部上拉，外部 10k 更好 */
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);
}

static void dht11_pull_low_release(void)
{
	DHT11_IO_OUT();            /* 切换为输出 */
	DHT11_DQ_OUT(0);           /* 拉低 DQ */
	HAL_Delay(DHT11_START_LOW_MS);
	DHT11_DQ_OUT(1);           /* DQ = 1 释放总线 */
}

void DHT11_Rst(void)
{
	dht11_pull_low_release();
	DHT11_DELAY_US(30);        /* 主机拉高 20~40us，等待 DHT11 应答 */
}

uint8_t DHT11_Check(void)
{
	uint16_t retry = 0;

	DHT11_IO_IN();             /* 切换为输入，读取 DHT11 的响应 */

	/* 等待 DHT11 把总线拉低 */
	while ((DHT11_DQ_IN == GPIO_PIN_SET) && (retry < DHT11_RESPONSE_TIMEOUT_US))
	{
		retry++;
		DHT11_DELAY_US(1);
	}
	if (retry >= DHT11_RESPONSE_TIMEOUT_US)
		return 1;

	/* 等待 80us 低电平结束 */
	retry = 0;
	while ((DHT11_DQ_IN == GPIO_PIN_RESET) && (retry < DHT11_RESPONSE_TIMEOUT_US))
	{
		retry++;
		DHT11_DELAY_US(1);
	}
	if (retry >= DHT11_RESPONSE_TIMEOUT_US)
		return 1;

	/* 等待 80us 高电平结束，结束后紧接着就是第 1 位数据 */
	retry = 0;
	while ((DHT11_DQ_IN == GPIO_PIN_SET) && (retry < DHT11_RESPONSE_TIMEOUT_US))
	{
		retry++;
		DHT11_DELAY_US(1);
	}
	if (retry >= DHT11_RESPONSE_TIMEOUT_US)
		return 1;

	return 0;
}

uint8_t DHT11_Read_Bit(void)
{
	uint16_t retry = 0;

	/* 等待上一位/响应信号的高电平结束 */
	while ((DHT11_DQ_IN == GPIO_PIN_SET) && (retry < DHT11_BIT_TIMEOUT_US))
	{
		retry++;
		DHT11_DELAY_US(1);
	}
	if (retry >= DHT11_BIT_TIMEOUT_US)
	{
		s_dht11_timeout = 1;
		return 0;
	}

	/* 等待本位的 50us 低电平结束（上升沿） */
	retry = 0;
	while ((DHT11_DQ_IN == GPIO_PIN_RESET) && (retry < DHT11_BIT_TIMEOUT_US))
	{
		retry++;
		DHT11_DELAY_US(1);
	}
	if (retry >= DHT11_BIT_TIMEOUT_US)
	{
		s_dht11_timeout = 1;
		return 0;
	}

	/* 上升沿之后 40us 采样：0 位（26~28us）已回低，1 位（70us）仍为高 */
	DHT11_DELAY_US(40);

	if (DHT11_DQ_IN == GPIO_PIN_SET)
		return 1;
	else
		return 0;
}

uint8_t DHT11_Read_Byte(void)
{
	uint8_t i, dat;

	dat = 0;
	for (i = 0; i < 8; i++)
	{
		dat <<= 1;
		dat |= DHT11_Read_Bit();
	}
	return dat;
}

uint8_t DHT11_Read_Raw(uint8_t *buf)
{
	uint8_t i;
	uint32_t primask;
	if (buf == 0)
		return 1;

	/* 起始信号的低电平 20ms 保持中断开启 */
	dht11_pull_low_release();
	primask = __get_PRIMASK();       /* 保存原状态，避免在中断里调用时误开启 */
	__disable_irq();

	DHT11_DELAY_US(30);              /* 释放总线后的 20~40us 高电平 */
	if (DHT11_Check() != 0)
	{
		__set_PRIMASK(primask);
		return 1;                    /* 没有响应 */
	}

	s_dht11_timeout = 0;
	for (i = 0; i < 5; i++)          /* 读取 40 位数据 */
	{
		buf[i] = DHT11_Read_Byte();
	}
	__set_PRIMASK(primask);

	if (s_dht11_timeout)
		return 3;

	return 0;
}

uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
	uint8_t buf[5];
	uint8_t rc;

	if ((temp == 0) || (humi == 0))
		return 1;

	rc = DHT11_Read_Raw(buf);
	if (rc != 0)
		return rc;

	/* buf: [0]湿度整数 [1]湿度小数 [2]温度整数 [3]温度小数 [4]校验和 */
	if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
		return 2;                    /* 校验失败（原代码这里会误报成功） */

	*humi = buf[0];
	*temp = buf[2];

	return 0;
}

uint8_t DHT11_Init(void)
{
	DHT11_DWT_Init();                /* 使能 DWT，供微秒延时使用 */

	DHT11_IO_OUT();                  /* 引脚配置成推挽输出（CubeMX 已配过，这里兜底） */
	DHT11_DQ_OUT(1);                 /* 空闲时保持高电平 */

	HAL_Delay(DHT11_POWER_ON_WAIT_MS);   /* 上电后等待 1s 让内部稳定 */

	DHT11_Rst();                     /* 复位 DHT11 */
	return DHT11_Check();            /* 等待 DHT11 的回应 */
}
