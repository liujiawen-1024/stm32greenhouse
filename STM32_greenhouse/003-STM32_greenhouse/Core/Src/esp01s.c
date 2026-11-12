#include "esp01s.h"
#include "usart.h"
#include "string.h"
#include "app_state.h"

// 中断专用的后台接收缓冲区
static char rx_buf[256] = {0};
static uint16_t rx_idx = 0;
static uint8_t rx_byte = 0;

/******轻量 JSON 取值******/
#define JF_KEY_MAX_LEN      12
#define YTH_TEMP_MIN        (-40)
#define YTH_TEMP_MAX        100
#define YTH_PCT_MAX         100
#define YTH_FLAG_MAX        1

typedef enum {
    JF_I8 = 0,
    JF_U8
} jf_type_t;

typedef struct {
    const char *key;
    void       *dst;
    jf_type_t   type;
    int32_t     lo;
    int32_t     hi;
    uint8_t     manual_only;   /* 1 = 仅手动模式下接受 */
} jf_field_t;

/******字段表******/
static const jf_field_t k_jf_fields[] = {
    { "Ytemp",   &g_app.env_yth.ytemp_th, JF_I8, YTH_TEMP_MIN, YTH_TEMP_MAX, 0u },
    { "Yhumi",   &g_app.env_yth.yhumi_th, JF_U8, 0,            YTH_PCT_MAX,  0u },
    { "Yill",    &g_app.env_yth.yill_th,  JF_U8, 0,            YTH_PCT_MAX,  0u },
    { "Ysoil",   &g_app.env_yth.ysoil_th, JF_U8, 0,            YTH_PCT_MAX,  0u },
    { "CMode",   &g_app.ctrl_mode,        JF_U8, 0,            YTH_FLAG_MAX, 0u },
    { "Warning", &g_app.alarm,            JF_U8, 0,            YTH_FLAG_MAX, 0u },
    { "fan",     &g_app.env_sw.fan,       JF_U8, 0,            YTH_FLAG_MAX, 1u },
    { "pump",    &g_app.env_sw.pump,      JF_U8, 0,            YTH_FLAG_MAX, 1u },
    /* [待补充] 第 9 个字段确认后在此加一行 */
};

#define JF_FIELD_CNT  (sizeof(k_jf_fields) / sizeof(k_jf_fields[0]))

static uint8_t Serial_RxPacket[64];

// 等待函数
static uint8_t ESP01S_WaitACK(const char *expect, uint32_t timeout) {
    const uint32_t t0 = HAL_GetTick();

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

static const char *json_skip_ws(const char *p, const char *limit)
{
    while ((p < limit) && ((*p == ' ') || (*p == '\t'))) { p++; }
    return p;
}

/* 从 '{' 找到配对的 '}'（跳过字符串内的括号与转义），失败返回 NULL */
static const char *json_match_brace(const char *p)
{
    int32_t depth = 0;
    uint8_t in_str = 0u;

    while (*p != '\0') {
        const char c = *p;
        if (in_str != 0u) {
            if ((c == '\\') && (p[1] != '\0')) { p += 2; continue; }
            if (c == '"') { in_str = 0u; }
        } else {
            if (c == '"')      { in_str = 1u; }
            else if (c == '{') { depth++; }
            else if (c == '}') {
                depth--;
                if (depth == 0) { return p; }
            }
        }
        p++;
    }
    return NULL;
}

/* 定位 "key" 的值首字符；兼容 {"value":N} 一层对象包装 */
static const char *json_value_start(const char *json, const char *limit, const char *key)
{
    char pat[JF_KEY_MAX_LEN + 3];
    const size_t klen = strlen(key);
    const char *p;

    if (klen > JF_KEY_MAX_LEN) { return NULL; }

    pat[0] = '"';
    memcpy(&pat[1], key, klen);
    pat[klen + 1] = '"';
    pat[klen + 2] = '\0';

    p = strstr(json, pat);
    if ((p == NULL) || (p >= limit)) { return NULL; }

    p = json_skip_ws(p + klen + 2, limit);
    if ((p >= limit) || (*p != ':')) { return NULL; }
    p = json_skip_ws(p + 1, limit);

    if ((p < limit) && (*p == '{')) {                 /* {"value":N} 形式 */
        const char *q = json_value_start(p, limit, "value");
        if (q != NULL) { return q; }
    }
    return p;
}

/* 十进制整数：成功 1；缺失/非法/溢出返回 0 */
static uint8_t json_atoi(const char *p, const char *limit, int32_t *out)
{
    int32_t sign = 1;
    int32_t val  = 0;
    uint8_t digits = 0u;

    if ((p == NULL) || (p >= limit)) { return 0u; }

    if (*p == '-')      { sign = -1; p++; }
    else if (*p == '+') { p++; }

    while ((p < limit) && (*p >= '0') && (*p <= '9')) {
        if (val > (INT32_MAX - 9) / 10) { return 0u; }
        val = (val * 10) + (int32_t)(*p - '0');
        p++;
        digits++;
    }
    if (digits == 0u) { return 0u; }

    *out = (sign < 0) ? -val : val;
    return 1u;
}

static int32_t jf_clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/* 定位 params 对象体 [begin, end)，成功 1 */
static uint8_t json_params_scope(const char *json, const char **begin, const char **end)
{
    const char *limit = json + strlen(json);
    const char *p = json_value_start(json, limit, "params");
    const char *q;

    if ((p == NULL) || (p >= limit) || (*p != '{')) { return 0u; }

    q = json_match_brace(p);
    if (q == NULL) { return 0u; }

    *begin = p;
    *end   = q;
    return 1u;
}

void ESP01S_ParseMQTTMessage(void)
{
    const char *msg;
    const char *body;
    const char *body_end;
    uint8_t updated = 0u;

    if (Serial_RxFlag == 0u) { return; }
    Serial_RxFlag = 0u;

    msg = (const char *)Serial_RxPacket;

    /* 只处理属性下发；自己上报的回显（+MQTTPUB 那行也含 params）不当命令 */
    if (strstr(msg, "+MQTTPUB") != NULL) { return; }
    if (strstr(msg, "params") == NULL)   { return; }

    if (json_params_scope(msg, &body, &body_end) == 0u) {
        printf("mqtt: no params\r\n");
        return;
    }

    for (uint32_t i = 0u; i < JF_FIELD_CNT; i++) {
        const jf_field_t *f = &k_jf_fields[i];
        int32_t v;

        if ((f->manual_only != 0u) && (g_app.ctrl_mode == 0u)) {
            continue;                     /* 自动模式下开关由 APP_Data_Ctrl() 决定 */
        }
        if (json_atoi(json_value_start(body, body_end, f->key), body_end, &v) == 0u) {
            continue;                     /* 字段缺失或非法：保持原值 */
        }
        v = jf_clamp(v, f->lo, f->hi);    /* 超范围钳到上下限，不拒绝 */

        if (f->type == JF_I8) {
            *(int8_t *)f->dst  = (int8_t)v;
        } else {
            *(uint8_t *)f->dst = (uint8_t)v;
        }
        updated++;
    }

    printf("mqtt params ok:%u\r\n", (unsigned)updated);
}

// 中断回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // 关键字状态机 ， 根据接收到的数据更新状态机状态
        static uint8_t RxState   = 0;   // 当前状态机状态的静态变量
        static uint8_t pRxPacket = 0;   // 当前接收数据位置的静态变量
        uint8_t RxData = rx_byte;
        if (RxState == 0)
        {
            if (RxData == '+' && Serial_RxFlag == 0)
            {
                Serial_RxPacket[0] = '+';
                RxState = 1;
                pRxPacket = 1;
            }
        }
        else if (RxState == 1)
        {
            if (RxData == '\r')
                RxState = 2;
            else if (pRxPacket < sizeof(Serial_RxPacket) - 1)
                Serial_RxPacket[pRxPacket++] = RxData;
        }
        else if (RxState == 2)
        {
            if (RxData == '\n')
            {
                RxState = 0;
                Serial_RxPacket[pRxPacket] = '\0';
                Serial_RxFlag = 1;
            }
            else
                RxState = 0;
        }
        // 关键字监听 ， WIFI掉线时触发
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
