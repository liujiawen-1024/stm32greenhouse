#include "app_state.h"

/* 全局定时采集标志：唯一定义点（对应 app_state.h 中的 extern 声明） */
__IO uint8_t g_get_data_flag = 0u;
__IO uint8_t Serial_RxFlag = 0;

app_state_t g_app = {
    .page = PAGE_HOME,
    .cursor_th = 0,
    .cursor_sw = 0,
    .ctrl_mode = 0,  /* 0 自动 1 手动 */
    .alarm = 1,      /* 0 报警 1 正常 */
    .online = 0,     /* 0 离线 1 在线 */
    .manual_tick = 0u,
    .env_data = {0},
    .env_th = {25,60,30,40},
    .env_yth = {25,60,30,40},
    .env_sw = {0}
};

void APP_STATE_Init(void) {
    // 函数留空 ， 为存flash做准备
}
