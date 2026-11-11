#include "app.h"

#include "app_event.h"
#include "app_state.h"
#include "bh1750.h"
#include "dht11.h"
#include "esp01s.h"
#include "gpio.h"
#include "oled.h"

__IO static uint8_t data_add = 0 ;
__IO static uint8_t data_sub = 0 ;
__IO static uint8_t data_sw = 0 ;

void APP_Init(void) {
    OLED_Init();
    OLED_CLS();
    OLED_ShowStr(0, 0, (unsigned char *)"sensor init...", 2);
    DHT11_Init();
    BH1750_Init();
    HAL_Delay(100*10);
    OLED_ShowStr(0, 2, (unsigned char *)"cloud connecting", 2);
    g_app.online = ESP01S_Init();
    OLED_ShowStr(0, 5, (unsigned char *)"boot up!!!", 2);
    OLED_CLS();
    OLED_ALL();
    printf("Init OK\r\n");
    printf("AT\r\n");
}

void APP_Key(void) {
    const uint8_t key = EV_KeyTake();

    for (uint8_t i = 0; i < 6; i++) {
        if (key & (1 << i)) {
            switch (i+1) {
                case 1: if (g_app.cursor_th != 0) {data_add = 1;}break;
                case 2: if (g_app.cursor_th != 0) {data_sub = 1;}break;
                case 3: g_app.page%= 4;g_app.page++;g_app.cursor_th=0;g_app.cursor_sw=0;break;
                case 4: if (g_app.page == 3) {g_app.cursor_sw = 0;g_app.cursor_th %= 4;g_app.cursor_th++;}
                        if (g_app.page == 4) {g_app.cursor_th = 1;g_app.cursor_sw %= 6;g_app.cursor_sw++;}break;
                case 5: g_app.ctrl_mode = !g_app.ctrl_mode;break;
                case 6: if (g_app.cursor_th != 0) {data_sw = 1;}
                        if (g_app.ctrl_mode && g_app.cursor_th != 1){g_app.alarm = !g_app.alarm;}
                    break;
                default:g_app.page=0;g_app.cursor_th=0;g_app.cursor_sw=0;break;
            }
        }
    }
    if (g_app.page == 3) {
        switch (g_app.cursor_th) {
            case 1:
                if (data_add == 1) {data_add = 0;
                    if (g_app.env_th.temp_th< 60) g_app.env_th.temp_th++;}
                if (data_sub == 1) {data_sub = 0;
                    if (g_app.env_th.temp_th > -20) g_app.env_th.temp_th--;}
                break;
            case 2:
                if (data_add == 1) {data_add = 0;
                    if (g_app.env_th.humi_th < 100) g_app.env_th.humi_th++;}
                if (data_sub == 1) {data_sub = 0;
                    if (g_app.env_th.humi_th > 0) g_app.env_th.humi_th--;}
                break;
            case 3:
                if (data_add == 1) {data_add = 0;
                    if (g_app.env_th.ill_th < 100) g_app.env_th.ill_th++;}
                if (data_sub == 1) {data_sub = 0;
                    if (g_app.env_th.ill_th > 0) g_app.env_th.ill_th--;}
                break;
            case 4:
                if (data_add == 1) {data_add = 0;
                    if (g_app.env_th.soil_th < 100) g_app.env_th.soil_th++;}
                if (data_sub == 1) {data_sub = 0;
                    if (g_app.env_th.soil_th > 0) g_app.env_th.soil_th--;}
                break;
            default:
                break;
        }
    }
    if (g_app.page == 4) {
        switch (g_app.cursor_sw) {
            case 1:if (data_sw == 1){data_sw = 0;g_app.alarm = !g_app.alarm;} break;
            case 2:if (data_sw == 1){data_sw = 0;g_app.ctrl_mode = !g_app.ctrl_mode;} break;
            case 3:if (data_sw == 1){data_sw = 0;g_app.env_sw.fan = !g_app.env_sw.fan;} break;
            case 4:if (data_sw == 1){data_sw = 0;g_app.env_sw.pump = !g_app.env_sw.pump;} break;
            case 5:if (data_sw == 1){data_sw = 0;g_app.env_sw.humi = !g_app.env_sw.humi;} break;
            case 6:if (data_sw == 1){data_sw = 0;g_app.env_sw.ill  = !g_app.env_sw.ill ;} break;
            default:break;
        }
    }
}
