#ifndef VM_H
#define VM_H

#include "phys_mem.h"

#include <stdint.h>

#define L1_NUM_PAGES 4096
#define L2_NUM_PAGES 256 // 1MB section / 4KB page = 256 pages

#define RAM_PAGE_END 0x200 // 512
#define PERIPHERAL_PAGE_END 0x210 // 528

#define KERNEL_VBASE 0xC0000000 // 0b1100...

enum {
    KERNEL_VBASE_RAM  = KERNEL_VBASE,
    KERNEL_VBASE_PERI = KERNEL_VBASE + TOTAL_MEM  // 0xE0000000 = vbase + 512MB
};

#define __va(paddr) ((void*) ((uintptr_t) (paddr) | KERNEL_VBASE))
#define __pa(vaddr) (((uintptr_t) (vaddr) & ~KERNEL_VBASE))

// Page tables (arm1176 p6-39)
typedef union {
    uint32_t val;
    struct {
        uint32_t tag  : 2;  // always 0b10
        uint32_t B    : 1;  // bufferable
        uint32_t C    : 1;  // cacheable
        uint32_t XN   : 1;  // execute-never
        uint32_t dom  : 4;  // domain
        uint32_t IMP  : 1;  // implementation-defined
        uint32_t AP   : 2;  // access perms
        uint32_t TEX  : 3;  // type extensions
        uint32_t APX  : 1;  // access perm extensions
        uint32_t S    : 1;  // shareable (multi-core)
        uint32_t nG   : 1;  // non-global
        uint32_t sup  : 1;  // regular section or supersection
        uint32_t NS   : 1;  // not secure (just set to 0)
        uint32_t addr : 12; // physical address
    };
} L1_Sec_PTE;

typedef union {
    uint32_t val;
    struct {
        uint32_t XN   : 1;  // execute-never
        uint32_t sb1  : 1;  // always 1
        uint32_t B    : 1;  // bufferable
        uint32_t C    : 1;  // cacheable
        uint32_t AP   : 2;  // access perms
        uint32_t TEX  : 3;  // type extensions
        uint32_t APX  : 1;  // access perm extensions
        uint32_t S    : 1;  // shareable (multi-core)
        uint32_t nG   : 1;  // non-global
        uint32_t addr : 20; // physical address
    };
} L2_Small_PTE;

enum {
    AP_RW        = 0b11, // sup/user RW
    AP_NO_ACCESS = 0b00,
    AP_RD        = 0b10,
    AP_SUPER     = 0b01, // sup RW, user no acc
};

enum {
    DOM_no_access = 0b00,
    DOM_client    = 0b01,
    DOM_reserved  = 0b10,
    DOM_manager   = 0b11
};

extern volatile uint32_t l1_page_table[L1_NUM_PAGES];

void l1_page_table_init();
volatile void* l2_page_table_init();
void map_page_4k(volatile uint32_t* l1_pt_vaddr, uint32_t vaddr, uint32_t paddr, int is_user);
uint32_t unmap_page_4k(volatile uint32_t* l1_pt_vaddr, uint32_t vaddr);

#endif
