#include "app_event.h"
#include "main.h"

__IO static uint8_t s_key_event      = 0;
__IO static uint8_t s_sample_pending = 0;

void EV_KeyPost(uint8_t idx) {
    if (idx < KEY_IDX_MAX) s_key_event = (uint8_t)(1u << idx);
}

void EV_SamplePost(void) {
    s_sample_pending = 1;
}

/* 读-改-写必须原子，否则 读到之后、清零之前 到来的新事件会被吞掉 */
static uint8_t take_and_clear(volatile uint8_t *p)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint8_t v = *p;
    *p = 0u;
    __set_PRIMASK(primask);   /* 恢复而不是无脑 __enable_irq */
    return v;
}

uint8_t EV_KeyTake(void)    { return take_and_clear(&s_key_event); }
uint8_t EV_SampleTake(void) { return take_and_clear(&s_sample_pending); }
