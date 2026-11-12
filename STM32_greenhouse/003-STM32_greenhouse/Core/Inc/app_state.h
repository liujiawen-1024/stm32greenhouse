#ifndef APP_STATE_H
#define APP_STATE_H

// 业务状态的唯一定义点与初始化
#include "stdint.h"
#include "main.h"
/* 注意：此处只能声明为 extern，唯一实体在 app_state.c 中定义。
   若写成 static，每个包含本头文件的 .c 都会得到各自独立的一份副本，
   ISR 里置 1、main.c 里永远读到 0，条件判断将永远不成立。 */
extern __IO uint8_t g_get_data_flag; // 1 表示获取数据，0 表示不获取数据

/* 数据模型 */
typedef struct {
    int8_t  temp  ;   // 温度
    uint8_t humi  ;   // 湿度
    uint8_t ill   ;   // 光照强度
    uint8_t soil  ;   // 土壤湿度
}env_data_t;

/* 阈值模型 */
typedef struct {
    int8_t  temp_th  ;   // 温度阈值
    uint8_t humi_th  ;   // 湿度阈值
    uint8_t ill_th   ;   // 光照强度阈值
    uint8_t soil_th  ;   // 土壤湿度阈值
}env_th_t;

/* 云端阈值模型 */
typedef struct {
    int8_t  ytemp_th  ;   // 温度阈值
    uint8_t yhumi_th  ;   // 湿度阈值
    uint8_t yill_th   ;   // 光照强度阈值
    uint8_t ysoil_th  ;   // 土壤湿度阈值
}env_yth_t;

/* 开关模型 */
typedef struct {
    uint8_t fan   ;   // 风扇
    uint8_t pump  ;   // 水泵
    uint8_t humi  ;   // 加湿器
    uint8_t ill   ;   // 补光灯
}env_sw_t;

/* 页面模型 */
typedef enum  {
    PAGE_HOME = 1 ,
    PAGE_DATA = 2 ,
    PAGE_TH   = 3 ,
    PAGE_SW   = 4 ,
    PAGE_MAX  = 5
}page_t;

/* 唯一实例 */
typedef struct {
    /* UI部分 */
    page_t page;
    uint8_t cursor_th;
    uint8_t cursor_sw;

    /* 模型与状态 */
    uint8_t ctrl_mode;
    uint8_t alarm;
    uint8_t online;

    /* 业务数据 */
    env_data_t env_data;
    env_th_t env_th;
    env_yth_t env_yth;
    env_sw_t env_sw;

}app_state_t;

extern app_state_t g_app;

void APP_STATE_Init(void);

#endif // APP_STATE_H
