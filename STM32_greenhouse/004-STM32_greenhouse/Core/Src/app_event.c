#include "app_event.h"
#include "main.h"

/* 只保留最后一个按键：后到的直接覆盖先到的 */
static __IO uint8_t s_key_last = 0u;
static __IO uint8_t s_key_flag = 0u;

void EV_KeyPost(uint8_t idx) {
    if (idx >= KEY_IDX_MAX) {
        return;
    }
    s_key_last = idx;   /* 覆盖：多键同按只保留最后一个 */
    s_key_flag = 1u;
}

uint8_t EV_KeyTake(uint8_t *idx) {
    uint32_t primask;
    uint8_t  has;

    if (idx == 0) {
        return 0u;
    }

    /* 取走时关中断，避免"刚读到 flag 就被 ISR 覆盖"的丢失 */
    primask = __get_PRIMASK();
    __disable_irq();

    has = s_key_flag;
    if (has != 0u) {
        *idx = s_key_last;
        s_key_flag = 0u;
    }

    __set_PRIMASK(primask);   /* 恢复原状态，不在中断里误开中断 */

    return has;
}
