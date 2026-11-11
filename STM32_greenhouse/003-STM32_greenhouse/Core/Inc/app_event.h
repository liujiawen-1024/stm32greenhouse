#ifndef APP_EVENT_H
#define APP_EVENT_H

// 主循环的事件通道（按键 ， 采样）

#include <stdint.h>

/* 按键位序号 ， 和 EXTI 回调里的 idx 一一对应 */
#define KEY_IDX_1   0u
#define KEY_IDX_2   1u
#define KEY_IDX_3   2u
#define KEY_IDX_4   3u
#define KEY_IDX_5   4u
#define KEY_IDX_6   5u
#define KEY_IDX_MAX 6u

/* 生产者 只在中断上下文调用 */
void EV_KeyPost(uint8_t idx);
void EV_SamplePost(void);

/* 消费者 只在主循环调用 取走自动清零 */
uint8_t EV_KeyTake(void);       // 返回 6bit 位图
uint8_t EV_SampleTake(void);    // 返回 1 表示有一次采集请求

#endif // APP_EVENT_H
