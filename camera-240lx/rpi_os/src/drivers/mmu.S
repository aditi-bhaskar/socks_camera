#include "armv6_cache_asm.h"

// void mmu_init() - disable extended pages, set translation table base, configure domain 0
.global mmu_init
.extern __l1_page_table_start__
mmu_init:
    INV_ALL_CACHES(r0)
    INV_TLB(r0)
    DSB(r0)

    PREFETCH_FLUSH(r0)
    
    mov r1, #0x1
    DOMAIN_CTRL_WR(r1) // set for domain 0 to client mode

    ldr r1, =__l1_page_table_start__
    bic r1, r1, #0xC0000000  // convert to paddr
    mov r2, #0
    TTBR0_SET(r1)
    TTBR1_SET(r2)
    TTBR_BASE_CTRL_WR(r2)

    CONTROL_REG1_RD(r1)
    orr r1, r1, #(1 << 23)  // turn on XP bit of control reg (disable extended pages)
    CONTROL_REG1_WR(r1)

    FLUSH_BTB(r0)
    PREFETCH_FLUSH(r0)

    bx lr

// void mmu_enable() - arm1176 p6-9
.global mmu_enable
mmu_enable:
    CONTROL_REG1_RD(r1)
    bic r1, r1, #(1 << 12)  // disable icache (zero out bit 12)
    CONTROL_REG1_WR(r1)
    PREFETCH_FLUSH(r0)

    INV_ICACHE(r0)
    DSB(r0)
    orr r1, r1, #1    // turn on MMU (turn on bit 0)
    CONTROL_REG1_WR(r1)
    PREFETCH_FLUSH(r0)

    FLUSH_BTB(r0)
    PREFETCH_FLUSH(r0)

    bx lr

// void mmu_disable()
.global mmu_disable
mmu_disable:
    CLEAN_INV_DCACHE(r0)
    DSB(r0)

    CONTROL_REG1_RD(r1)
    bic r1, r1, #1   // turn off MMU (clear bit 0)
    CONTROL_REG1_WR(r1)
    PREFETCH_FLUSH(r0)

    INV_ICACHE(r0)
    DSB(r0)

    FLUSH_BTB(r0)
    PREFETCH_FLUSH(r0)

    bx lr

// void mmu_enable_caches()
.global mmu_enable_caches
mmu_enable_caches:
    CONTROL_REG1_RD(r1)
    orr r1, r1, #(1 << 2)  // dcache
    orr r1, r1, #(1 << 11) // BTB
    orr r1, r1, #(1 << 12) // icache
    CONTROL_REG1_WR(r1)

    PREFETCH_FLUSH(r0)

    bx lr

// void mmu_flush_dcache()
.global mmu_flush_dcache
mmu_flush_dcache:
    CLEAN_INV_DCACHE(r0)
    DSB(r0)

    bx lr

// void mmu_flush_caches()

// void mmu_disable_caches()

// void mmu_sync_ptes()
.global mmu_sync_ptes
mmu_sync_ptes:
    CLEAN_INV_DCACHE(r0)
    INV_ICACHE(r0)
    DSB(r0)

    INV_TLB(r0)
    DSB(r0)

    FLUSH_BTB(r0)
    PREFETCH_FLUSH(r0)

    bx lr

// void set_cp15_ctrl_reg(CP15CtrlReg reg)
.global set_cp15_ctrl_reg
set_cp15_ctrl_reg:
    CONTROL_REG1_WR(r0)
    PREFETCH_FLUSH(r1)
    bx lr

// CP15CtrlReg get_cp15_ctrl_reg()
.global get_cp15_ctrl_reg
get_cp15_ctrl_reg:
    CONTROL_REG1_RD(r0)
    bx lr
