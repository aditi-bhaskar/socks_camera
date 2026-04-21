#include "armv6_asm.h"
#include "armv6_cache_asm.h"

.global proc_run
proc_run:
    INV_ALL_CACHES(r2)
    INV_TLB(r2)
    DSB(r2)

    PREFETCH_FLUSH(r2)

    TTBR0_SET(r1)
    FLUSH_BTB(r2)
    PREFETCH_FLUSH(r2)

    ldr r2, [r0, #64]
    msr spsr_cxsf, r2
    ldr lr, [r0, #60]

    cps #SYSTEM_MODE
    ldr sp, [r0, #52]  // user SP
    cps #SUPER_MODE
    
    ldmia r0, {r0-r12}
    
    movs pc, lr
