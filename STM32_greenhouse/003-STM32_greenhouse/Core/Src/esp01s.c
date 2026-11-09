#include "esp01s.h"
#include "usart.h"
#include "string.h"
#include "app_state.h"

// 中断专用的后台接收缓冲区
static char rx_buf[256] = {0};
static uint16_t rx_idx = 0;
static uint8_t rx_byte = 0;

// 等待函数
static uint8_t ESP01S_WaitACK(const char *expect, uint32_t timeout) {
    uint32_t t0 = HAL_GetTick();

    // 发指令前，清空中断缓冲
    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));

    while ((HAL_GetTick() - t0) < timeout) {
        // 只要有中断收进来的数据，直接对比
        if (strstr(rx_buf, expect) != NULL) {
            return 1;
        }
        // 顺便检测一下是否掉线
        if (strstr(rx_buf, "WIFI DISCONNECT") != NULL) {
            g_app.online = 0;
            return 0;
        }
    }
    return 0;
}

// 清空函数
static void ESP01S_FlushRx(void) {
    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
}

uint8_t ESP01S_Init(void) {
    printf("ESP01S_Init\r\n");
    char cmd[32];

    // 开启串口接收中断
    ESP01S_FlushRx();
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    /* 1. 发 AT，等 OK */
    strcpy(cmd, "AT\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) { printf("AT fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 2. 发 AT+RST，等 OK */
    strcpy(cmd, "AT+RST\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    if (!ESP01S_WaitACK("OK", 5000)) { printf("RST fail\r\n"); return 0; }
    HAL_Delay(5000);
    ESP01S_FlushRx();

    /* 3. 发 ATE0，等 OK */
    strcpy(cmd, "ATE0\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) { printf("ATE0 fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 4. 发 AT+CWMODE=1，等 OK */
    strcpy(cmd, "AT+CWMODE=1\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) { printf("CWMODE fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 5. 发 AT+CWDHCP=1,1，等 OK */
    strcpy(cmd, "AT+CWDHCP=1,1\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) { printf("CWDHCP fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 6. 发 ESP01S_WIFI，等 OK */
    HAL_UART_Transmit(&huart2, (uint8_t *)ESP01S_WIFI, strlen(ESP01S_WIFI), 1000);
    if (!ESP01S_WaitACK("OK", 5000)) { printf("ESP01S_WIFI fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 7. 发 MQTT_USERCFG，等 OK */
    HAL_UART_Transmit(&huart2, (uint8_t *)MQTT_USERCFG, strlen(MQTT_USERCFG), 1000);
    if (!ESP01S_WaitACK("OK", 8000)) { printf("MQTT_USERCFG fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 8. 发 MQTT_CONN，等 OK */
    HAL_UART_Transmit(&huart2, (uint8_t *)MQTT_CONN, strlen(MQTT_CONN), 1000);
    if (!ESP01S_WaitACK("OK", 8000)) { printf("MQTT_CONN fail\r\n"); return 0; }
    ESP01S_FlushRx();

    /* 9. 发 MQTT_SUB_SET，等 OK */
    HAL_UART_Transmit(&huart2, (uint8_t *)MQTT_SUB_SET, strlen(MQTT_SUB_SET), 1000);
    if (!ESP01S_WaitACK("OK", 5000)) { printf("MQTT_SUB_SET fail\r\n"); return 0; }
    ESP01S_FlushRx();

    printf("ESP01S_Init OK\r\n");
    g_app.online = 1; // 初始化成功，标记在线
    return 1;
}

void ESP01S_Send_Data(void) {
    char send_buf[256] = {0};
    sprintf(send_buf, MQTT_PUB_HEAD
              "\\\"temp\\\":{\\\"value\\\":%d}\\,"
              "\\\"humi\\\":{\\\"value\\\":%d}\\,"
              "\\\"ill\\\":{\\\"value\\\":%d}\\,"
              "\\\"soil\\\":{\\\"value\\\":%d}"
              MQTT_PUB_TAIL, g_app.env_data.temp, g_app.env_data.humi, g_app.env_data.ill, g_app.env_data.soil);

    HAL_UART_Transmit(&huart2, (uint8_t *)send_buf, strlen(send_buf), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) {
        printf("Send Data fail\r\n");
        g_app.online = 0; // 发送失败也当做掉线处理
        return;
    }
    ESP01S_FlushRx();
}

void ESP01S_Send_TH(void) {
    char send_buf[256] = {0};
    sprintf(send_buf, MQTT_PUB_HEAD
              "\\\"temp_th\\\":{\\\"value\\\":%d}\\,"
              "\\\"humi_th\\\":{\\\"value\\\":%d}\\,"
              "\\\"ill_th\\\":{\\\"value\\\":%d}\\,"
              "\\\"soil_th\\\":{\\\"value\\\":%d}"
              MQTT_PUB_TAIL,
              g_app.env_th.temp_th, g_app.env_th.humi_th, g_app.env_th.ill_th, g_app.env_th.soil_th);

    HAL_UART_Transmit(&huart2, (uint8_t *)send_buf, strlen(send_buf), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) {
        printf("Send TH fail\r\n");
        g_app.online = 0;
        return;
    }
    ESP01S_FlushRx();
}

void ESP01S_Send_SW(void) {
    char send_buf[256] = {0};
    sprintf(send_buf, MQTT_PUB_HEAD
              "\\\"fan\\\":{\\\"value\\\":%d}\\,"
              "\\\"pump\\\":{\\\"value\\\":%d}\\,"
              "\\\"humi\\\":{\\\"value\\\":%d}\\,"
              "\\\"ill\\\":{\\\"value\\\":%d}"
              MQTT_PUB_TAIL,
              g_app.env_sw.fan, g_app.env_sw.pump, g_app.env_sw.humi, g_app.env_sw.ill);

    HAL_UART_Transmit(&huart2, (uint8_t *)send_buf, strlen(send_buf), 1000);
    if (!ESP01S_WaitACK("OK", 1000)) {
        printf("Send SW fail\r\n");
        g_app.online = 0;
        return;
    }
    ESP01S_FlushRx();
}

// 中断回调（只负责搬运和抓取关键字，极速退出）
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (rx_idx < sizeof(rx_buf) - 1) {
            rx_buf[rx_idx++] = rx_byte;
            rx_buf[rx_idx] = '\0';
        }

        // 后台随时监听掉线
        if (strstr(rx_buf, "WIFI DISCONNECT") != NULL) {
            g_app.online = 0;
            rx_idx = 0;
            memset(rx_buf, 0, sizeof(rx_buf));
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}