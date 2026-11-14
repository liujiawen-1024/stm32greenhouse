#ifndef APP_EVENT_H
#define APP_EVENT_H

/*
 * 按键事件通道：只保留最后一个按键。
 * 后按下的按键会覆盖先按下的，所以"多键同按"最终只有最后一个生效。
 *   生产者：HAL_GPIO_EXTI_Callback（中断上下文，只写普通变量）
 *   消费者：ControlTask
 */

#include <stdint.h>

/* 按键序号，和 EXTI 回调里的映射一一对应 */
#define KEY_IDX_1   0u
#define KEY_IDX_2   1u
#define KEY_IDX_3   2u
#define KEY_IDX_4   3u
#define KEY_IDX_5   4u
#define KEY_IDX_6   5u
#define KEY_IDX_MAX 6u

/* 按键消抖时间(ms)，EXTI 回调里用 */
#define KEY_DEBOUNCE_MS  20u

/* 只在 EXTI 回调里调用 */
void EV_KeyPost(uint8_t idx);

/* 只在任务里调用：返回 1 表示取到一个按键，序号写入 *idx */
uint8_t EV_KeyTake(uint8_t *idx);

#endif // APP_EVENT_H
