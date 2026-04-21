#include "interrupt.h"
#include "arm_timer.h"
#include "lib.h"

void arm_timer_int_init(uint32_t prescale, uint32_t ncycles) {
    mem_barrier_dsb();
    PUT32(IRQ_BASIC_PENDING, ARM_TIMER_IRQ_PENDING);
    mem_barrier_dsb();
    PUT32(ARM_TIMER_LOAD, ncycles);

    uint32_t v = 0;
    switch(prescale) {
        case 1: v = ARM_TIMER_CTRL_PRESCALE_1; break;
        case 16: v = ARM_TIMER_CTRL_PRESCALE_16; break;
        case 256: v = ARM_TIMER_CTRL_PRESCALE_256; break;
        default: panic("Illegal prescale");
    }

    PUT32(ARM_TIMER_CTRL,
            ARM_TIMER_CTRL_23BIT |
            ARM_TIMER_CTRL_ENABLE |
            ARM_TIMER_CTRL_INT_ENABLE |
            v);

    mem_barrier_dsb();
}

void arm_timer_enable() {
    PUT32(ARM_TIMER_CTRL, ARM_TIMER_CTRL_INT_ENABLE |
            ARM_TIMER_CTRL_ENABLE | ARM_TIMER_CTRL_23BIT);
    PUT32(ARM_TIMER_PREDIV, 0);
    arm_timer_enable_irq();
}
void arm_timer_enable_irq() {
    PUT32(IRQ_ENABLE_BASIC, (1 << ARM_TIMER_IRQ_PENDING));
}
void arm_timer_disable_irq() {
    panic("Unimplemented");
}

void arm_timer_set_freq(uint32_t hz) {
    arm_timer_set_load(ARM_TIMER_CLOCK_RATE / hz);
}
void arm_timer_set_load(uint32_t count) {
    PUT32(ARM_TIMER_LOAD, count);
}

bool arm_timer_has_interrupt() {
    mem_barrier_dsb();
    bool res = arm_timer_int_pending() &&
        ((GET32(IRQ_BASIC_PENDING) >> ARM_TIMER_IRQ_PENDING) & 1);
    mem_barrier_dsb();
    return res;
}
bool arm_timer_int_pending() {
    return GET32(ARM_TIMER_MASK_IRQ) & 1;
}
void arm_timer_int_clear() {
    PUT32(ARM_TIMER_IRQ_CLR, 1);
}

