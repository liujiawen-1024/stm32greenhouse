/************************************************************************************
* File name: OLED_I2C.c
* Project  : 
* Processor: STM32F103C8T6
* Compiler : MDK fo ARM
*
* Description:128*64点阵的OLED显示屏驱动文件，SSD1306驱动IIC通信方式显示屏
*
* Others: none;
*
* Function List:
* 2. void I2C_WriteByte(uint8_t addr,uint8_t data) -- 向寄存器地址写一个byte的数据
* 3. void WriteCmd(unsigned char I2C_Command) -- 写命令
* 4. void WriteDat(unsigned char I2C_Data) -- 写数据
* 5. void OLED_Init(void) -- OLED屏初始化
* 6. void OLED_SetPos(unsigned char x, unsigned char y) -- 设置起始点坐标
* 7. void OLED_Fill(unsigned char fill_Data) -- 全屏填充
* 8. void OLED_CLS(void) -- 清屏
* 9. void OLED_ON(void) -- 唤醒
* 10. void OLED_OFF(void) -- 睡眠
* 11. void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize) -- 显示字符串(字体大小有6*8和8*16两种)
* 12. void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char N) -- 显示中文(中文需要先取模，然后放到codetab.h中)
* 13. void OLED_DrawBMP(unsigned char x0,unsigned char y0,unsigned char x1,unsigned char y1,unsigned char BMP[]) -- BMP图片
*
* History: none;
*
*************************************************************************************/

#include "i2c.h"
#include "oled.h"
#include "codetab.h"

static uint8_t LAST_PAGE = 0 ; // 历史界面 ，用于优化刷新

void WriteCmd(unsigned char I2C_Command)//写命令
{
	uint8_t  buf[2] ;
	buf[0] = 0x00;
	buf[1] = I2C_Command;
	// I2C_WriteByte(I2C1,OLED_ADDRESS,buf, 2);   替换这个函数 
	// OLED_ADDRESS 器件地址 
	// buf 发送的数据地址 
	// 2 :发送的字节数 
	// 1000 : 超时时间 
	HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, buf, 2, 1000); 
	
}

void WriteDat(unsigned char I2C_Data)//写数据
{	
	uint8_t buf[2] ;
	buf[0] = 0x40;
	buf[1] = I2C_Data;
	//I2C_WriteByte(I2C1,OLED_ADDRESS,buf, 2);
	HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, buf, 2, 1000); 
}

void OLED_Init(void)
{
	HAL_Delay(1); //这里的延时很重要 , 使用HAL库替换  , 延时至少是100us 
	
	WriteCmd(0xAE); //display off
	WriteCmd(0x20);	//Set Memory Addressing Mode	
	WriteCmd(0x10);	//00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	WriteCmd(0xb0);	//Set Page Start Address for Page Addressing Mode,0-7
	WriteCmd(0xc8);	//Set COM Output Scan Direction
	WriteCmd(0x00); //---set low column address
	WriteCmd(0x10); //---set high column address
	WriteCmd(0x40); //--set start line address
	WriteCmd(0x81); //--set contrast control register
	WriteCmd(0xff); //亮度调节 0x00~0xff
	WriteCmd(0xa1); //--set segment re-map 0 to 127
	WriteCmd(0xa6); //--set normal display
	WriteCmd(0xa8); //--set multiplex ratio(1 to 64)
	WriteCmd(0x3F); //
	WriteCmd(0xa4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	WriteCmd(0xd3); //-set display offset
	WriteCmd(0x00); //-not offset
	WriteCmd(0xd5); //--set display clock divide ratio/oscillator frequency
	WriteCmd(0xf0); //--set divide ratio
	WriteCmd(0xd9); //--set pre-charge period
	WriteCmd(0x22); //
	WriteCmd(0xda); //--set com pins hardware configuration
	WriteCmd(0x12);
	WriteCmd(0xdb); //--set vcomh
	WriteCmd(0x20); //0x20,0.77xVcc
	WriteCmd(0x8d); //--set DC-DC enable
	WriteCmd(0x14); //
	WriteCmd(0xaf); //--turn on oled panel
}

void OLED_SetPos(unsigned char x, unsigned char y) //设置起始点坐标
{ 
	WriteCmd(0xb0+y);
	WriteCmd(((x&0xf0)>>4)|0x10);
	WriteCmd((x&0x0f)|0x01);
}

void OLED_Fill(unsigned char fill_Data)//全屏填充
{
	unsigned char m,n;
	for(m=0;m<8;m++)
	{
		WriteCmd(0xb0+m);		//page0-page1
		WriteCmd(0x00);		//low column start address
		WriteCmd(0x10);		//high column start address
		for(n=0;n<128;n++)
			{
				WriteDat(fill_Data);
			}
	}
}

void OLED_CLS(void)//清屏
{
	OLED_Fill(0x00);
}

//--------------------------------------------------------------
// Prototype      : void OLED_ON(void)
// Calls          : 
// Parameters     : none
// Description    : 将OLED从休眠中唤醒
//--------------------------------------------------------------
void OLED_ON(void)
{
	WriteCmd(0X8D);  //设置电荷泵
	WriteCmd(0X14);  //开启电荷泵
	WriteCmd(0XAF);  //OLED唤醒
}

//--------------------------------------------------------------
// Prototype      : void OLED_OFF(void)
// Calls          : 
// Parameters     : none
// Description    : 让OLED休眠 -- 休眠模式下,OLED功耗不到10uA
//--------------------------------------------------------------
void OLED_OFF(void)
{
	WriteCmd(0X8D);  //设置电荷泵
	WriteCmd(0X10);  //关闭电荷泵
	WriteCmd(0XAE);  //OLED休眠
}

//--------------------------------------------------------------
// Prototype      : void OLED_ShowChar(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize)
// Calls          : 
// Parameters     : x,y -- 起始点坐标(x:0~127, y:0~7); ch[] -- 要显示的字符串; TextSize -- 字符大小(1:6*8 ; 2:8*16)
// Description    : 显示codetab.h中的ASCII字符,有6*8和8*16可选择
//--------------------------------------------------------------
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char TextSize)
{
	unsigned char c = 0,i = 0,j = 0;
	switch(TextSize)
	{
		case 1:
		{
			while(ch[j] != '\0')
			{
				c = ch[j] - 32;
				if(x > 126)
				{
					x = 0;
					y++;
				}
				OLED_SetPos(x,y);
				for(i=0;i<6;i++)
					WriteDat(F6x8[c][i]);
				x += 6;
				j++;
			}
		}break;
		case 2:
		{
			while(ch[j] != '\0')
			{
				c = ch[j] - 32;
				if(x > 120)
				{
					x = 0;
					y++;
				}
				OLED_SetPos(x,y);
				for(i=0;i<8;i++)
					WriteDat(F8X16[c*16+i]);
				OLED_SetPos(x,y+1);
				for(i=0;i<8;i++)
					WriteDat(F8X16[c*16+i+8]);
				x += 8;
				j++;
			}
		}break;
	}
}

//--------------------------------------------------------------
// Prototype      : void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char N)
// Calls          : 
// Parameters     : x,y -- 起始点坐标(x:0~127, y:0~7); N:汉字在codetab.h中的F16x16[]的下标
// Description    : 显示codetab.h中的汉字,16*16点阵
// 这个屏幕最多可以显示4行汉字  , 每一行可以 显示8个汉字  , 这个屏幕最多可以显示32个汉字  
//--------------------------------------------------------------
void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char N)
{
	unsigned char wm=0;
	unsigned int  adder=32*N;
	OLED_SetPos(x , y);
	for(wm = 0;wm < 16;wm++)
	{
		WriteDat(F16x16[adder]);
		adder += 1;
	}
	OLED_SetPos(x,y + 1);
	for(wm = 0;wm < 16;wm++)
	{
		WriteDat(F16x16[adder]);
		adder += 1;
	}
}

//--------------------------------------------------------------
// Prototype      : void OLED_DrawBMP(unsigned char x0,unsigned char y0,unsigned char x1,unsigned char y1,unsigned char BMP[]);
// Calls          : 
// Parameters     : x0,y0 -- 起始点坐标(x0:0~127, y0:0~7); x1,y1 -- 起点对角线(结束点)的坐标(x1:1~128,y1:1~8)
// Description    : 显示BMP位图
//--------------------------------------------------------------
void OLED_DrawBMP(unsigned char x0,unsigned char y0,unsigned char x1,unsigned char y1,unsigned char BMP[])
{
	unsigned int j=0;
	unsigned char x,y;

  if(y1%8==0)
		y = y1/8;
  else
		y = y1/8 + 1;
	for(y=y0;y<y1;y++)
	{
		OLED_SetPos(x0,y);
    for(x=x0;x<x1;x++)
		{
			WriteDat(BMP[j++]);
		}
	}
}

// 把有符号数转成十进制字符串，返回长度
static uint8_t IntToStr(int16_t val, char *buf)
{
	char tmp[8];
	uint8_t n = 0, i = 0;
	uint16_t uval;

	if (val < 0) {
		buf[i++] = '-';
		uval = (uint16_t)(-val);
	} else {
		uval = (uint16_t)val;
	}

	if (uval == 0) {
		tmp[n++] = '0';
	} else {
		while (uval > 0) { tmp[n++] = '0' + (uval % 10); uval /= 10; }
	}
	while (n > 0) { buf[i++] = tmp[--n]; }

	// 补空格到固定 3 位（右对齐：空格补前面）
	// 注意：'-' 不算数字位，所以前面要插入空格，不能覆盖 '-'
	while (i < 3) {
		// 把已有内容整体右移 1 位，空出 buf[0]
		for (uint8_t k = i; k > 0; k--) buf[k] = buf[k - 1];
		buf[0] = ' ';
		i++;
	}

	buf[i] = '\0';
	return i;
}

void OLED_ShowHome(const app_state_t *st) {
	/*****************智慧农业************************/
	const uint8_t title[4] = {0,1,2,3};
	for (int i = 0; i < 4; i++) OLED_ShowCN(32+(i<<4), 0, title[i]);
	/*****************当前状态************************/
	const uint8_t state[4] = {4,5,6,7};
	const uint8_t state_condition[4] = {8,9,10,11};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4, 2, state[i]);
	const uint8_t state_base = st->alarm ? 0 : 2;
	for (int i = 0; i < 2; i++) OLED_ShowCN(80+(i<<4), 2, state_condition[i+state_base]);
	/*****************控制模式************************/
	const uint8_t ctrl[4] = {12,13,14,15};
	const uint8_t ctrl_mode[4] = {16,18,17,18};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4, 4, ctrl[i]);
	const uint8_t ctrl_base = st->ctrl_mode ? 0 : 2;
	for (int i = 0; i < 2; i++) OLED_ShowCN(80+(i<<4), 4, ctrl_mode[i+ctrl_base]);
	/*****************在线情况************************/
	const uint8_t online[4] = {19,20,21,22};
	const uint8_t online_status[4] = {19,20,23,20};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4, 6, online[i]);
	const uint8_t online_base = st->online ? 0 : 2;
	for (int i = 0; i < 2; i++) OLED_ShowCN(80+(i<<4), 6, online_status[i+online_base]);
}

void OLED_ShowData(const app_state_t *st) {
	/*********环境数据*********/
	const uint8_t env[4] = {33,34,35,36};
	for (int i = 0; i < 4; i++) OLED_ShowCN(32+(i<<4), 0, env[i]);
	/******温度******/
	const uint8_t temp_arr[2] = {24,26};
	for (int i = 0; i < 2; i++) OLED_ShowCN(i<<4,2,temp_arr[i]);
	char buf[4];
	IntToStr(st->env_data.temp, buf);
	OLED_ShowStr(36, 2, (unsigned char*)buf, 2);
	/******湿度******/
	const uint8_t humi_arr[2] = {25,26};
	for (int i = 0; i < 2; i++) OLED_ShowCN(64+(i<<4),2,humi_arr[i]);
	IntToStr(st->env_data.humi, buf);
	OLED_ShowStr(100, 2, (unsigned char*)buf, 2);
	/******光照强度******/
	const uint8_t ill_arr[4] = {27,28,37,26};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4,4,ill_arr[i]);
	IntToStr(st->env_data.ill, buf);
	OLED_ShowStr(68, 4, (unsigned char*)buf, 2);
	/******土壤湿度******/
	const uint8_t soil_arr[4] = {29,30,25,26};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4,6,soil_arr[i]);
	IntToStr(st->env_data.soil, buf);
	OLED_ShowStr(68, 6, (unsigned char*)buf, 2);
}

void OLED_ShowDataTH(const app_state_t *st) {
	/*********数据阈值*********/
	const uint8_t env[4] = {35,36,38,39};
	for (int i = 0; i < 4; i++) OLED_ShowCN(32+(i<<4), 0, env[i]);
	/******温度******/
	const uint8_t temp_arr[2] = {24,26};
	for (int i = 0; i < 2; i++) OLED_ShowCN(i<<4,2,temp_arr[i]);
	char buf[4];
	IntToStr(st->env_th.temp_th, buf);
	OLED_ShowStr(36, 2, (unsigned char*)buf, 2);
	/******湿度******/
	const uint8_t humi_arr[2] = {25,26};
	for (int i = 0; i < 2; i++) OLED_ShowCN(64+(i<<4),2,humi_arr[i]);
	IntToStr(st->env_th.humi_th, buf);
	OLED_ShowStr(100, 2, (unsigned char*)buf, 2);
	/******光照强度******/
	const uint8_t ill_arr[4] = {27,28,37,26};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4,4,ill_arr[i]);
	IntToStr(st->env_th.ill_th, buf);
	OLED_ShowStr(68, 4, (unsigned char*)buf, 2);
	/******土壤湿度******/
	const uint8_t soil_arr[4] = {29,30,25,26};
	for (int i = 0; i < 4; i++) OLED_ShowCN(i<<4,6,soil_arr[i]);
	IntToStr(st->env_th.soil_th, buf);
	OLED_ShowStr(68, 6, (unsigned char*)buf, 2);
}

void OLED_ShowState(const app_state_t *st) {
	/*********开关***********/
	const uint8_t onoff[2] = {49,48};
	/*********正常报警手动自动***********/
	const uint8_t alarm[8] = {8,9,10,11,16,18,17,18};
	uint8_t state_base = st->alarm ? 0 : 2 ;
	uint8_t ctrl_base = st->ctrl_mode ? 0 : 2 ;
	for (int i = 0; i < 2; i++) OLED_ShowCN(16+(i<<4), 0, alarm[i+state_base]);
	for (int i = 0; i < 2; i++) OLED_ShowCN(80+(i<<4), 0, alarm[i+4+ctrl_base]);
	/***********风扇水泵**************/
	const uint8_t fan[2] = {40,41};
	for (int i = 0; i < 2; i++) OLED_ShowCN(i<<4, 2, fan[i]);
	const uint8_t water[2] = {42,43};
	for (int i = 0; i < 2; i++) OLED_ShowCN(64+(i<<4), 2, water[i]);
	for (int i = 0; i < 1; i++) OLED_ShowCN(40+(i<<4), 2, onoff[i+st->env_sw.fan]);
	for (int i = 0; i < 1; i++) OLED_ShowCN(104+(i<<4), 2, onoff[i+st->env_sw.pump]);
	/************加湿器***************/
	const uint8_t humi[3] = {44,25,45};
	for (int i = 0; i < 3; i++) OLED_ShowCN(i<<4, 4, humi[i]);
	for (int i = 0; i < 1; i++) OLED_ShowCN(64+(i<<4), 4, onoff[i+st->env_sw.humi]);
	/************补光灯***************/
	const uint8_t ill[3] = {46,47,48};
	for (int i = 0; i < 3; i++) OLED_ShowCN(i<<4, 6, ill[i]);
	for (int i = 0; i < 1; i++) OLED_ShowCN(64+(i<<4), 6, onoff[i+st->env_sw.ill]);
}

void OLED_SetData(const app_state_t *st) {
	if (st->page == 3) {
		switch (st->cursor_th) {
			case 1:OLED_ShowStr(120,6,(unsigned char*)" ",2);OLED_ShowStr(5,0,(unsigned char*)"V",2);break;
			case 2:OLED_ShowStr(5,0,(unsigned char*)" ",2);OLED_ShowStr(115,0,(unsigned char*)"V",2);break;
			case 3:OLED_ShowStr(115,0,(unsigned char*)" ",2);OLED_ShowStr(120,4,(unsigned char*)"<",2);break;
			case 4:OLED_ShowStr(120,4,(unsigned char*)" ",2);OLED_ShowStr(120,6,(unsigned char*)"<",2);break;
			default:break;
		}
	}
	if (st->page == 4) {
		switch (st->cursor_sw) {
			case 1:OLED_ShowStr(80,6,(unsigned char*)" ",2);OLED_ShowStr(48,0,(unsigned char*)"<",2);break;
			case 2:OLED_ShowStr(48,0,(unsigned char*)" ",2);OLED_ShowStr(112,0,(unsigned char*)"<",2);break;
			case 3:OLED_ShowStr(112,0,(unsigned char*)" ",2);OLED_ShowStr(56,2,(unsigned char*)"<",2);break;
			case 4:OLED_ShowStr(56,2,(unsigned char*)" ",2);OLED_ShowStr(120,2,(unsigned char*)"<",2);break;
			case 5:OLED_ShowStr(120,2,(unsigned char*)" ",2);OLED_ShowStr(80,4,(unsigned char*)"<",2);break;
			case 6:OLED_ShowStr(80,4,(unsigned char*)" ",2);OLED_ShowStr(80,6,(unsigned char*)"<",2);break;
			default:break;
		}
	}
}

void OLED_ALL(void) {
	if (g_app.page != LAST_PAGE) {
		OLED_CLS();
		LAST_PAGE = g_app.page;
	}
	switch(g_app.page) {
		case 1:OLED_ShowHome(&g_app);break;
		case 2:OLED_ShowData(&g_app);break;
		case 3:OLED_ShowDataTH(&g_app);break;
		case 4:OLED_ShowState(&g_app);break;
		default:OLED_ShowHome(&g_app);break;
	}
}
