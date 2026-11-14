#ifndef APP_H
#define APP_H

#include "main.h"

#define MANUAL_HOLD_MS   60000u   /* 手动控制保持期：1 分钟 */

void APP_Init(void);
void APP_Key(void);
void APP_Data_Ctrl(void);
void APP_GET_DATA(void);
void APP_STORE(void);
void APP_Read(void);

/* 任意一次手动动作：进入手动并刷新 1 分钟计时 */
void APP_ManualTouch(void);
/* 主循环每圈调用：手动期满自动回到自动模式 */
void APP_ManualPoll(void);

#endif //APP_H
