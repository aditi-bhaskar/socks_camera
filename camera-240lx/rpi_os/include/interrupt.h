#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "vm.h"

#define IRQ_BASE (0x2000B000 | KERNEL_VBASE)

// 1 is for 0-31 on peripherals table, 2 is for 32-63
enum {
    IRQ_BASIC_PENDING = IRQ_BASE + 0x200,
    IRQ_PENDING_1     = IRQ_BASE + 0x204,
    IRQ_PENDING_2     = IRQ_BASE + 0x208,
    IRQ_ENABLE_1      = IRQ_BASE + 0x210,
    IRQ_ENABLE_2      = IRQ_BASE + 0x214,
    IRQ_ENABLE_BASIC  = IRQ_BASE + 0x218,
    IRQ_DISABLE_1     = IRQ_BASE + 0x21C,
    IRQ_DISABLE_2     = IRQ_BASE + 0x220
};

typedef enum {
    AUX_INT     = 29,
    CAM1_INT    = 39, // https://github.com/rsta2/circle/blob/master/include/circle/bcm2835int.h
    GPIO_INT_0  = 49,
    GPIO_INT_1  = 50,
} ARM_Peripherals_Table;

typedef enum {
    ARM_TIMER_IRQ_PENDING,
} ARM_Basic_Pending_Reg;

#endif
