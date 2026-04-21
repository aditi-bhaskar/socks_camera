#ifndef ARMV6_CACHE_ASM_H
#define ARMV6_CACHE_ASM_H

// from CS140E

#define DSB(Rd)             mcr p15, 0, Rd, c7, c10, 4
#define DMB(Rd)             mcr p15, 0, Rd, c7, c10, 5 

#define PREFETCH_FLUSH(Rd)  mcr p15, 0, Rd, c7, c5, 4  
#define FLUSH_BTB(Rd)       mcr p15, 0, Rd, c7, c5, 6

#define INV_ICACHE(Rd)                                           \
    mcr p15, 0, Rd, c7, c5, 0   ; /* invalidate entire I-cache */   \
    mcr p15, 0, Rd, c7, c5, 0;  ; /* invalidate entire I-cache */   \
    mcr p15, 0, Rd, c7, c5, 0;  ; /* invalidate entire I-cache */   \
    mcr p15, 0, Rd, c7, c5, 0;  ; /* invalidate entire I-cache */   \
    .rept   11                  ; /* ARM Ltd recommends at least 11 nops */\
    nop                         ;                                   \
    .endr

#define CLEAN_INV_DCACHE(Rd)    mcr p15, 0, Rd, c7, c14, 0  
#define INV_DCACHE(Rd)          mcr p15, 0, Rd, c7, c6, 0  
#define INV_ALL_CACHES(Rd) INV_ICACHE(Rd); INV_DCACHE(Rd)

#define INV_ITLB(Rd)        mcr p15, 0, Rd, c8, c5, 0 
#define INV_DTLB(Rd)        mcr p15, 0, Rd, c8, c6, 0 
#define INV_TLB(Rd)         mcr p15, 0, Rd, c8, c7, 0

#define TTBR0_GET(Rd)           mrc p15, 0, Rd, c2, c0, 0
#define TTBR0_SET(Rd)           mcr p15, 0, Rd, c2, c0, 0
#define TTBR1_GET(Rd)           mrc p15, 0, Rd, c2, c0, 1
#define TTBR1_SET(Rd)           mcr p15, 0, Rd, c2, c0, 1
#define TTBR_BASE_CTRL_RD(Rd)   mrc p15, 0, Rd, c2, c0, 2
#define TTBR_BASE_CTRL_WR(Rd)   mcr p15, 0, Rd, c2, c0, 2

#define ASID_SET(Rd)            mcr p15, 0, Rd, c13, c0, 1
#define ASID_GET(Rd)            mrc p15, 0, Rd, c13, c0, 1

#define DOMAIN_CTRL_RD(Rd)      mrc p15, 0, Rd, c3, c0, 0
#define DOMAIN_CTRL_WR(Rd)      mcr p15, 0, Rd, c3, c0, 0

#define CACHE_TYPE_RD(Rd)       mrc p15, 0, Rd, c0, c0, 1
#define TLB_CONFIG_RD(Rd)       mrc p15, 0, Rd, c0, c0, 3

#define CONTROL_REG1_RD(Rd) mrc p15, 0, Rd, c1, c0, 0
#define CONTROL_REG1_WR(Rd) mcr p15, 0, Rd, c1, c0, 0

#define FAULT_STATUS_REG_GET(Rd) mrc p15, 0, Rd, c5, c0, 0

#endif
