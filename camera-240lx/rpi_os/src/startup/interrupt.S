.p2align 5
.global swi_ptr
.global irq_ptr
.global _interrupt_table
_interrupt_table:
    ldr pc, reset_ptr
    ldr pc, undef_ptr
    ldr pc, swi_ptr
    ldr pc, prefetch_ptr
    ldr pc, data_abort_ptr
    ldr pc, reset_ptr
    ldr pc, irq_ptr
    ldr pc, fiq_ptr

reset_ptr:      .word reset_vector
undef_ptr:      .word undef_asm
swi_ptr:        .word swi_asm
prefetch_ptr:   .word prefetch_asm
data_abort_ptr: .word data_abort_asm
irq_ptr:        .word interrupt_vector
fiq_ptr:        .word fast_interrupt_vector

reset_vector:
    bl rpi_reset

data_abort_asm:
    // adjust LR to pc that crashed
    sub lr, lr, #8

    push {r0-r3, r12, lr}
    mov r0, lr

    // read dfsr (crash reason) and dfar (crash address)
    mrc p15, 0, r1, c5, c0, 0
    mrc p15, 0, r2, c6, c0, 0

    b data_abort_vector

undef_asm:
    sub lr, lr, #4
    push {r0-r3, r12, lr}
    mov r0, lr
    b undefined_instruction_vector

prefetch_asm:
    sub lr, lr, #4
    push {r0-r3, r12, lr}
    mov r0, lr
    b prefetch_abort_vector

swi_asm:
    // subtract sizeof(TrapFrame) + store all registers
    sub sp, sp, #72
    stmia sp, {r0-r12}^
    add r0, sp, #52
    stmia r0, {sp, lr}^
    str lr, [sp, #60] // user lr = SVC pc
    mrs r0, spsr
    str r0, [sp, #64]

    mov r0, sp
    bl swi_syscall_handler

    // restore registers
    ldr r0, [sp, #64]
    msr spsr_cxsf, r0
    ldr lr, [sp, #60]
    add r0, sp, #52
    ldmia r0, {sp, lr}^
    ldmia sp, {r0-r12}^
    add sp, sp, #72

    movs pc, lr
