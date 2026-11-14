#include "app.h"

#include "adc.h"
#include "app_event.h"
#include "app_state.h"
#include "bh1750.h"
#include "dht11.h"
#include "esp01s.h"
#include "gpio.h"
#include "oled.h"
#include "tim.h"

__IO static uint8_t data_add = 0 ;
__IO static uint8_t data_sub = 0 ;
__IO static uint8_t data_sw = 0 ;

/******回差设置******/
#define TEMP_HYSTERESIS  2
#define HUMI_HYSTERESIS  10
#define ILL_HYSTERESIS   10
#define SOIL_HYSTERESIS  10

/******影子变量******/
static uint8_t shadow_temp = 0;
static uint8_t shadow_humi = 0;
static uint8_t shadow_ill  = 0;
static uint8_t shadow_soil = 0;

void APP_Init(void) {
    HAL_TIM_Base_Start_IT(&htim3);
    OLED_Init();
    OLED_CLS();
    OLED_ShowStr(0, 0, (unsigned char *)"sensor init...", 2);
    DHT11_Init();
    BH1750_Init();
    APP_Read();
    OLED_ShowStr(0, 2, (unsigned char *)"cloud connecting", 2);
    g_app.online = ESP01S_Init();
    OLED_ShowStr(0, 5, (unsigned char *)"boot up!!!", 2);
    OLED_CLS();
    OLED_ALL();
}

void APP_Key(void) {
    const uint8_t key = EV_KeyTake();
    uint8_t last_page = g_app.page;

    for (uint8_t i = 0; i < 6; i++) {
        if (key & (1 << i)) {
            switch (i+1) {
                case 1: if (g_app.cursor_th != 0) {data_add = 1;}break;
                case 2: if (g_app.cursor_th != 0) {data_sub = 1;}break;
                case 3: g_app.page%= 4;g_app.page++;g_app.cursor_th=0;g_app.cursor_sw=0;break;
                case 4: if (g_app.page == 3) {g_app.cursor_sw = 0;g_app.cursor_th %= 4;g_app.cursor_th++;}
                        if (g_app.page == 4) {g_app.cursor_th = 1;g_app.cursor_sw %= 6;g_app.cursor_sw++;}break;
                case 5: if (g_app.ctrl_mode == 0u) { APP_ManualTouch(); }   /* 切手动并起计时 */
                        else { g_app.ctrl_mode = 0u; }                     /* 显式切回自动 */
                    break;
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
            case 2:if (data_sw == 1){data_sw = 0;
                        if (g_app.ctrl_mode == 0u) { APP_ManualTouch(); }  /* 切手动并开始计时 */
                        else { g_app.ctrl_mode = 0u; }                    /* 显式切回自动 */
                    } break;
            case 3:if (data_sw == 1&&g_app.ctrl_mode == 1){data_sw = 0;g_app.env_sw.fan_on  = !g_app.env_sw.fan_on ;APP_ManualTouch();} break;
            case 4:if (data_sw == 1&&g_app.ctrl_mode == 1){data_sw = 0;g_app.env_sw.pump_on = !g_app.env_sw.pump_on;APP_ManualTouch();} break;
            case 5:if (data_sw == 1&&g_app.ctrl_mode == 1){data_sw = 0;g_app.env_sw.mist_on = !g_app.env_sw.mist_on;APP_ManualTouch();} break;
            case 6:if (data_sw == 1&&g_app.ctrl_mode == 1){data_sw = 0;g_app.env_sw.lamp_on  = !g_app.env_sw.lamp_on ;APP_ManualTouch();} break;
            default:break;
        }
    }
    if (last_page != 3 && g_app.page == 3) {
        shadow_temp = g_app.env_th.temp_th;
        shadow_humi = g_app.env_th.humi_th;
        shadow_ill  = g_app.env_th.ill_th;
        shadow_soil = g_app.env_th.soil_th;
    }

    // 2. 刚刚离开阈值设置页 ， 检查并保存
    if (last_page == 3 && g_app.page != 3) {
        if (g_app.env_th.temp_th != shadow_temp ||
            g_app.env_th.humi_th != shadow_humi ||
            g_app.env_th.ill_th  != shadow_ill  ||
            g_app.env_th.soil_th != shadow_soil) {

            APP_STORE(); // 写入 EEPROM

            // 更新影子变量
            shadow_temp = g_app.env_th.temp_th;
            shadow_humi = g_app.env_th.humi_th;
            shadow_ill  = g_app.env_th.ill_th;
            shadow_soil = g_app.env_th.soil_th;
            }
    }
}

void APP_Data_Ctrl(void) {
    const uint8_t is_auto = (g_app.ctrl_mode == 0u) ? 1u : 0u;
    uint8_t is_normal = 1u;   /* 1 = 正常 */

    // 温度控制
    if (g_app.env_data.temp >= g_app.env_th.temp_th) {   // 超过上限
        is_normal = 0u;                                   // 温度异常
        if (is_auto != 0u) { g_app.env_sw.fan_on = 1u; }     // 仅自动模式驱动风扇
    } else if (g_app.env_data.temp <= (g_app.env_th.temp_th - TEMP_HYSTERESIS)) {
        if (is_auto != 0u) { g_app.env_sw.fan_on = 0u; }     // 低于下限：关风扇
    }
    // 湿度控制
    if (g_app.env_data.humi <= g_app.env_th.humi_th) {   // 低于下限
        is_normal = 0u;
        if (is_auto != 0u) { g_app.env_sw.mist_on = 1u; }
    } else if (g_app.env_data.humi >= (g_app.env_th.humi_th + HUMI_HYSTERESIS)) {
        if (is_auto != 0u) { g_app.env_sw.mist_on = 0u; }
    }
    // 光照控制
    if (g_app.env_data.ill <= g_app.env_th.ill_th) {     // 低于下限
        is_normal = 0u;
        if (is_auto != 0u) { g_app.env_sw.lamp_on = 1u; }
    } else if (g_app.env_data.ill >= (g_app.env_th.ill_th + ILL_HYSTERESIS)) {
        if (is_auto != 0u) { g_app.env_sw.lamp_on = 0u; }
    }
    // 土壤控制
    if (g_app.env_data.soil <= g_app.env_th.soil_th) {   // 低于下限
        is_normal = 0u;
        if (is_auto != 0u) { g_app.env_sw.pump_on = 1u; }
    } else if (g_app.env_data.soil >= (g_app.env_th.soil_th + SOIL_HYSTERESIS)) {
        if (is_auto != 0u) { g_app.env_sw.pump_on = 0u; }
    }

    // 报警由传感器决定；手动模式下不覆盖，避免冲掉用户手动设的报警位
    if (is_auto != 0u) {
        g_app.alarm = is_normal;
    }
}

/* 任意一次手动动作：进入手动并刷新 1 分钟计时 */
void APP_ManualTouch(void) {
    g_app.ctrl_mode   = 1u;
    g_app.manual_tick = HAL_GetTick();
}

/* 主循环每圈调用：手动期满自动回到自动模式 */
void APP_ManualPoll(void) {
    if (g_app.ctrl_mode == 0u) {
        return;
    }
    if ((uint32_t)(HAL_GetTick() - g_app.manual_tick) >= MANUAL_HOLD_MS) {
        g_app.ctrl_mode = 0u;    // 下一圈 APP_Data_Ctrl() 立即按阈值重新驱动
    }
}

void APP_GET_DATA(void) {
    DHT11_Read_Data((uint8_t *)&g_app.env_data.temp, (uint8_t *)&g_app.env_data.humi);
    Light_Intensity((uint8_t *)&g_app.env_data.ill);
    ADC_GetSoilVal((uint8_t *)&g_app.env_data.soil);
}

void APP_STORE(void) {
    uint8_t arr_th[4];
    arr_th[0] = g_app.env_th.temp_th;
    arr_th[1] = g_app.env_th.humi_th;
    arr_th[2] = g_app.env_th.ill_th;
    arr_th[3] = g_app.env_th.soil_th;

    if (HAL_I2C_Mem_Write(&hi2c1, 0x50 << 1, 0x00, I2C_MEMADD_SIZE_8BIT, arr_th, 4, 100) == HAL_OK) {
        HAL_Delay(5);
    }
}

void APP_Read(void) {
    uint8_t read_th[4] = {0};

    // 从 EEPROM 0x00 地址读取 4 个字节
    if (HAL_I2C_Mem_Read(&hi2c1, 0x50 << 1, 0x00, I2C_MEMADD_SIZE_8BIT, read_th, 4, 100) == HAL_OK) {

        // 核心逻辑：合法性检查
        if (read_th[0] <= 60 && read_th[1] <= 100 && read_th[2] <= 100 && read_th[3] <= 100) {

            // 数据合法：从 EEPROM 赋值
            g_app.env_th.temp_th = (int8_t)read_th[0];
            g_app.env_th.humi_th = read_th[1];
            g_app.env_th.ill_th  = read_th[2];
            g_app.env_th.soil_th = read_th[3];

        } else {
            // 数据非法：恢复为你设定的 yth 备份默认值
            g_app.env_th.temp_th = g_app.env_yth.ytemp_th;
            g_app.env_th.humi_th = g_app.env_yth.yhumi_th;
            g_app.env_th.ill_th  = g_app.env_yth.yill_th;
            g_app.env_th.soil_th = g_app.env_yth.ysoil_th;

            // 把默认值存进 EEPROM，下次就不用再恢复了
            APP_STORE();
        }
    } else {
        // 读取失败（I2C 没通）：同样恢复为 yth 备份值，保证系统能跑
        g_app.env_th.temp_th = g_app.env_yth.ytemp_th;
        g_app.env_th.humi_th = g_app.env_yth.yhumi_th;
        g_app.env_th.ill_th  = g_app.env_yth.yill_th;
        g_app.env_th.soil_th = g_app.env_yth.ysoil_th;
    }
}
