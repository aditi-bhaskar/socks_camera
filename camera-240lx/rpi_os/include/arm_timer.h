#ifndef ARM_TIMER_H
#define ARM_TIMER_H

#include "vm.h"
#include <stdint.h>
#include <stdbool.h>

#define ARM_TIMER_BASE (0x2000B400 | KERNEL_VBASE)
#define ARM_TIMER_CLOCK_RATE 700000000 // 700 MHz

enum {
    ARM_TIMER_LOAD = ARM_TIMER_BASE,
    ARM_TIMER_VAL = ARM_TIMER_BASE + 0x04,
    ARM_TIMER_CTRL = ARM_TIMER_BASE + 0x08,
    ARM_TIMER_IRQ_CLR = ARM_TIMER_BASE + 0x0C,
    ARM_TIMER_RAW_IRQ = ARM_TIMER_BASE + 0x10,
    ARM_TIMER_MASK_IRQ = ARM_TIMER_BASE + 0x14,
    ARM_TIMER_RELOAD = ARM_TIMER_BASE + 0x18,
    ARM_TIMER_PREDIV = ARM_TIMER_BASE + 0x1C,
    ARM_TIMER_COUNTER = ARM_TIMER_BASE + 0x20,
};

// BCM p197
enum {
    ARM_TIMER_CTRL_23BIT        = (1 << 1),
    ARM_TIMER_CTRL_PRESCALE_1   = (0 << 2),
    ARM_TIMER_CTRL_PRESCALE_16  = (1 << 2),
    ARM_TIMER_CTRL_PRESCALE_256 = (2 << 2),
    ARM_TIMER_CTRL_INT_ENABLE   = (1 << 5),
    ARM_TIMER_CTRL_ENABLE       = (1 << 7),
};

void arm_timer_enable();
void arm_timer_enable_irq();
void arm_timer_disable_irq();

void arm_timer_set_freq(uint32_t hz);
void arm_timer_set_load(uint32_t count);

bool arm_timer_has_interrupt();
bool arm_timer_int_pending();
void arm_timer_int_clear();

void arm_timer_int_init(uint32_t prescale, uint32_t ncycles);

#endif
