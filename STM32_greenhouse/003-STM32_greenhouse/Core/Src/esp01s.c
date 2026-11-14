#include "esp01s.h"
#include "usart.h"
#include "string.h"
#include "app_state.h"
#include "app.h"

// 接收缓冲：环形原始日志 + 单帧组装（ISR 只做搬运，解析在主循环）
#define ESP01S_RAW_MAX      256u    /* 环形原始日志上限，给 ESP01S_WaitACK 用 */
#define ESP01S_FRAME_MAX    160u    /* 单帧上限，够放下 MQTTSUBRECV 报文 */

static char     rx_buf[ESP01S_RAW_MAX] = {0};
static uint16_t rx_head = 0u;           /* 环形写指针 */
static uint16_t rx_cnt  = 0u;           /* 环形已存字节数 */
static uint8_t  rx_byte = 0u;

static char     s_frame[ESP01S_FRAME_MAX] = {0};   /* 中断内组装中的帧 */
static uint16_t s_frame_len   = 0u;
static uint8_t  s_frame_state = 0u;                /* 0 空闲 / 1 收帧 / 2 已收 '\r' */
static uint8_t  s_frame_ovf   = 0u;                /* 1 = 本帧超长，作废 */

static __IO uint8_t Serial_RxDrop = 0u;            /* 丢弃帧计数，调试可读 */
static __IO uint8_t Serial_RxErr  = 0u;            /* 串口错误/重挂失败次数 */

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

typedef enum {
    JF_ROLE_DATA = 0,   /* 普通数据 */
    JF_ROLE_MODE,       /* 手自动模式位 */
    JF_ROLE_SWITCH      /* 执行器开关：收到即视为一次手动动作 */
} jf_role_t;

typedef struct {
    const char *key;
    void       *dst;
    jf_type_t   type;
    int32_t     lo;
    int32_t     hi;
    jf_role_t   role;
} jf_field_t;

/******字段表******/
static const jf_field_t k_jf_fields[] = {
    { "Ytemp",   &g_app.env_yth.ytemp_th, JF_I8, YTH_TEMP_MIN, YTH_TEMP_MAX, JF_ROLE_DATA   },
    { "Yhumi",   &g_app.env_yth.yhumi_th, JF_U8, 0,            YTH_PCT_MAX,  JF_ROLE_DATA   },
    { "Yill",    &g_app.env_yth.yill_th,  JF_U8, 0,            YTH_PCT_MAX,  JF_ROLE_DATA   },
    { "Ysoil",   &g_app.env_yth.ysoil_th, JF_U8, 0,            YTH_PCT_MAX,  JF_ROLE_DATA   },
    { "CMode",   &g_app.ctrl_mode,        JF_U8, 0,            YTH_FLAG_MAX, JF_ROLE_MODE   },
    { "Warning", &g_app.alarm,            JF_U8, 0,            YTH_FLAG_MAX, JF_ROLE_DATA   },
    { "fan",     &g_app.env_sw.fan_on,       JF_U8, 0,            YTH_FLAG_MAX, JF_ROLE_SWITCH },
    { "pump",    &g_app.env_sw.pump_on,      JF_U8, 0,            YTH_FLAG_MAX, JF_ROLE_SWITCH },
    { "mist",    &g_app.env_sw.mist_on,      JF_U8, 0,            YTH_FLAG_MAX, JF_ROLE_SWITCH },
    { "lamp",    &g_app.env_sw.lamp_on,      JF_U8, 0,            YTH_FLAG_MAX, JF_ROLE_SWITCH },
};

#define JF_FIELD_CNT  (sizeof(k_jf_fields) / sizeof(k_jf_fields[0]))

static uint8_t Serial_RxPacket[ESP01S_FRAME_MAX] = {0};

// 清空函数
static void ESP01S_FlushRx(void) {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    rx_head = 0u;
    rx_cnt  = 0u;
    memset(rx_buf, 0, sizeof(rx_buf));
    __set_PRIMASK(primask);
}

/* 环形缓冲写入：中断上下文调用 */
static void rx_push(uint8_t b)
{
    rx_buf[rx_head] = (char)b;
    rx_head = (uint16_t)((rx_head + 1u) % ESP01S_RAW_MAX);
    if (rx_cnt < ESP01S_RAW_MAX) { rx_cnt++; }
}

/* 环形缓冲里是否含 pat：主循环上下文调用 */
static uint8_t rx_contains(const char *pat)
{
    const uint16_t n = (uint16_t)strlen(pat);
    uint16_t i;
    uint16_t j;

    if ((n == 0u) || (rx_cnt < n)) { return 0u; }

    for (i = 0u; i <= (uint16_t)(rx_cnt - n); i++) {
        for (j = 0u; j < n; j++) {
            const uint16_t idx =
                (uint16_t)(((rx_head + ESP01S_RAW_MAX - rx_cnt) + i + j) % ESP01S_RAW_MAX);
            if (rx_buf[idx] != pat[j]) { break; }
        }
        if (j == n) { return 1u; }
    }
    return 0u;
}

/* 刚写入的字节是否构成 pat 的结尾：中断上下文调用，开销 O(strlen(pat)) */
static uint8_t rx_endswith(const char *pat)
{
    const uint16_t n = (uint16_t)strlen(pat);
    uint16_t i;

    if ((n == 0u) || (rx_cnt < n)) { return 0u; }

    for (i = 0u; i < n; i++) {
        const uint16_t idx = (uint16_t)((rx_head + ESP01S_RAW_MAX - n + i) % ESP01S_RAW_MAX);
        if (rx_buf[idx] != pat[i]) { return 0u; }
    }
    return 1u;
}

// 等待函数
static uint8_t ESP01S_WaitACK(const char *expect, uint32_t timeout) {
    const uint32_t t0 = HAL_GetTick();

    // 发指令前，清空中断缓冲
    ESP01S_FlushRx();

    while ((HAL_GetTick() - t0) < timeout) {
        // 只要有中断收进来的数据，直接对比（环形缓冲，不再依赖 '\0' 结尾）
        if (rx_contains(expect) != 0u) {
            return 1u;
        }
        // 顺便检测一下是否掉线
        if (rx_contains("WIFI DISCONNECT") != 0u) {
            g_app.online = 0u;
            return 0u;
        }
    }
    return 0u;
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
              "\\\"mist\\\":{\\\"value\\\":%d}\\,"
              "\\\"lamp\\\":{\\\"value\\\":%d}"
              MQTT_PUB_TAIL,
              g_app.env_sw.fan_on, g_app.env_sw.pump_on, g_app.env_sw.mist_on, g_app.env_sw.lamp_on);

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
    uint8_t manual_hit = 0u;

    if (Serial_RxFlag == 0u) { return; }

    msg = (const char *)Serial_RxPacket;

    /* 只处理属性下发；自己上报的回显（+MQTTPUB 那行也含 params）不当命令 */
    if (strstr(msg, "+MQTTPUB") == NULL) {
        if (strstr(msg, "params") != NULL) {
            if (json_params_scope(msg, &body, &body_end) == 0u) {
                printf("mqtt: no params\r\n");
            } else {
                for (uint32_t i = 0u; i < JF_FIELD_CNT; i++) {
                    const jf_field_t *f = &k_jf_fields[i];
                    int32_t v;

                    if (json_atoi(json_value_start(body, body_end, f->key), body_end, &v) == 0u) {
                        continue;                  /* 字段缺失或非法：保持原值 */
                    }
                    v = jf_clamp(v, f->lo, f->hi); /* 超范围钳到上下限，不拒绝 */

                    if (f->type == JF_I8) {
                        *(int8_t *)f->dst  = (int8_t)v;
                    } else {
                        *(uint8_t *)f->dst = (uint8_t)v;
                    }
                    updated++;

                    /* 开关指令 = 一次手动动作；CMode=1 也进手动，CMode=0 是显式交还自动 */
                    if ((f->role == JF_ROLE_SWITCH) ||
                        ((f->role == JF_ROLE_MODE) && (v != 0))) {
                        manual_hit = 1u;
                    }
                }

                if (manual_hit != 0u) {
                    APP_ManualTouch();         /* 进入手动 + 刷新 1 分钟计时 */
                }
                if (updated != 0u) {
                    printf("mqtt params ok:%u\r\n", (unsigned)updated);
                }
            }
        }
    }

    Serial_RxFlag = 0u;   /* 全部处理完才放行下一帧 */
}

// 中断回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    uint8_t b;

    /* 只处理 ESP-01S 所在的 USART2；USART1 由 _write() 阻塞发送，不开中断 */
    if (huart->Instance != USART2) { return; }

    b = rx_byte;

    /* 1. 原始日志进环形缓冲；掉线关键字只比较刚到的尾部，不在中断里 strstr */
    rx_push(b);
    if (rx_endswith("WIFI DISCONNECT") != 0u) {
        g_app.online = 0u;
    }

    /* 2. 单帧组装：'+' 起头，'\r' 结束，'\n' 提交 */
    switch (s_frame_state) {
    case 0u:
        if ((b == (uint8_t)'+') && (Serial_RxFlag == 0u)) {
            s_frame[0] = '+';
            s_frame_len = 1u;
            s_frame_ovf = 0u;
            s_frame_state = 1u;
        }
        break;

    case 1u:
        if (b == (uint8_t)'\r') {
            s_frame_state = 2u;
        } else if (s_frame_len < (ESP01S_FRAME_MAX - 1u)) {
            s_frame[s_frame_len] = (char)b;
            s_frame_len++;
        } else {
            s_frame_ovf = 1u;           /* 超长：本帧作废，不交付半截 JSON */
        }
        break;

    default:                            /* s_frame_state == 2 */
        if (b == (uint8_t)'\n') {
            if ((s_frame_ovf == 0u) && (Serial_RxFlag == 0u)) {
                s_frame[s_frame_len] = '\0';
                memcpy(Serial_RxPacket, s_frame, (size_t)s_frame_len + 1u);
                Serial_RxFlag = 1u;     /* 整帧就绪，交给 ESP01S_ParseMQTTMessage() */
            } else {
                Serial_RxDrop++;        /* 上一条未解析或本帧溢出，丢弃并计数 */
            }
        }
        s_frame_len   = 0u;
        s_frame_ovf   = 0u;
        s_frame_state = 0u;
        break;
    }

    /* 3. 重新挂起：所有路径末尾都要执行，否则串口静默 */
    if (HAL_UART_Receive_IT(&huart2, &rx_byte, 1u) != HAL_OK) {
        Serial_RxErr++;
    }
}

/* 溢出/帧错误/噪声：HAL 内部已结束接收，这里清标志并重新挂起 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART2) { return; }

    __HAL_UART_CLEAR_OREFLAG(huart);    /* F1 HAL：读 SR + DR 清 ORE */

    s_frame_len   = 0u;
    s_frame_ovf   = 0u;
    s_frame_state = 0u;

    if (HAL_UART_Receive_IT(&huart2, &rx_byte, 1u) != HAL_OK) {
        Serial_RxErr++;
    }
}
