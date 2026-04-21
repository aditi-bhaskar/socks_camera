#include "armv6_asm.h"
#include "armv6_cache_asm.h"

.section ".bss"
.align 4
    // 16KB for SVC stack
    .space 16384 
svc_stack_top:

    // 4KB for IRQ stack
    .space 4096
irq_stack_top:

    // 4KB for abort stack
    .space 4096
abt_stack_top:

    // 4KB for undef stack
    .space 4096
und_stack_top:


.section ".text.boot"
.global _start
_start:
    // enable access to cp10/cp11 (RMW set bits 20-23 in CPACR)
    mrc p15, 0, r0, c1, c0, 2
    orr r0, r0, #(0xF << 20)
    mcr p15, 0, r0, c1, c0, 2
    
    PREFETCH_FLUSH(r0)

    // turn on VFP (bit 30)
    mov r0, #0x40000000
    vmsr fpexc, r0

    ldr sp, =0x8000

    // initialize virtual memory
    bl l1_page_table_init
    bl mmu_sync_ptes
    bl mmu_init
    bl mmu_enable

    ldr pc, =higher_half_entry

higher_half_entry:
    // initialize stacks
    ldr sp, =svc_stack_top

    cps #IRQ_MODE
    ldr sp, =irq_stack_top

    cps #ABORT_MODE
    ldr sp, =abt_stack_top

    cps #UNDEF_MODE
    ldr sp, =und_stack_top

    cps #SUPER_MODE

    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0

    // clear bss
bss_loop:
    cmp r0, r1
    bge bss_done
    str r2, [r0], #4
    b bss_loop

bss_done:
    // load interrupt table
    ldr r0, =_interrupt_table
    mcr p15, 0, r0, c12, c0, 0
    
    DSB(r0)
    PREFETCH_FLUSH(r0)

    // clear page 0 in L1 PT
    ldr r0, =__l1_page_table_start__
    mov r1, #0
    str r1, [r0]
    bl mmu_sync_ptes

    // jump to C initialization
    b cstart

