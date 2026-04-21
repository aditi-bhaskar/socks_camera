#ifndef MMU_H
#define MMU_H

#include <stdint.h>

// arm-1176, B-45
typedef union {
    struct {
        uint32_t mmu_enbl         : 1; // MMU enable
        uint32_t align_fault_enbl : 1; // alignment fault
        uint32_t dcache_enbl      : 1; // L1 data cache
        uint32_t write_buf_enbl   : 1; // enablt write buffer
        uint32_t sbo0             : 3;
        uint32_t endian           : 1; // 0 = little, 1 = big
        uint32_t S                : 1; // deprecated, MMU 
        uint32_t R                : 1; // deprecated, ROM protection
        uint32_t F                : 1;
        uint32_t branch_pred_enbl : 1; // branch prediction
        uint32_t icache_enbl      : 1; // instruction cache
        uint32_t high_except_v    : 1; // high exception vectors
        uint32_t cache_replace    : 1; // 0 = normal (random), 1 = round robin
        uint32_t L4               : 1; // T bit for PC load instructions
        uint32_t DT               : 1; // deprecated, global enable for data TCM
        uint32_t sbz0             : 1;
        uint32_t IT               : 1; // deprecated, global enable for instruction TCM
        uint32_t SBZ1             : 2;
        uint32_t FI               : 1; // fast interrupt low-latency features
        uint32_t U                : 1; // enables unaligned data access
        uint32_t XP               : 1; // extended page tables for subpages
        uint32_t VE               : 1; // VIC interface to determine interrupt vectors
        uint32_t EE               : 1; // how E bit is set in CPSR on exception
        uint32_t res0             : 2;
        uint32_t TR               : 1; // TEX remap in MMU
        uint32_t FA               : 1; // enable force AP
        uint32_t res1             : 2;
    };
    uint32_t val;
} CP15CtrlReg;


void mmu_init();
void mmu_enable();
void mmu_disable();

void mmu_enable_caches();
void mmu_disable_caches();
void mmu_flush_dcache();
// void mmu_flush_caches();

void mmu_sync_ptes();

void set_cp15_ctrl_reg(CP15CtrlReg reg);
CP15CtrlReg get_cp15_ctrl_reg();

#endif
