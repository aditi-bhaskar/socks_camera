// port/rpi-inline-asm.h — minimal shim of libpi's rpi-inline-asm.h for the
// rpi_os build. circular.h needs gcc_mb() and cpsr_int_enabled().
#ifndef PORT_RPI_INLINE_ASM_H
#define PORT_RPI_INLINE_ASM_H

// compiler memory barrier (ordering only).
static inline void gcc_mb(void) { asm volatile("" ::: "memory"); }

// We run BT in polling mode with interrupts effectively off. circular.h only
// uses this to guard its blocking cq_pop against deadlock; bt.c always pushes
// before it pops, so cq_pop never actually blocks. Returning 1 keeps that
// safety check from panicking.
static inline int cpsr_int_enabled(void) { return 1; }

#endif // PORT_RPI_INLINE_ASM_H
