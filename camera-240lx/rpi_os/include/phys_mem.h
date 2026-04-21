#ifndef PHYS_MEM_H
#define PHYS_MEM_H

#include <stdint.h>

#define PMEM_MAX_REGIONS  8       // arbitrary
#define PAGE_SIZE         4096    // 4KB
#define SECTION_SIZE      1048576 // 1MB
#define NUM_PAGES         131072  // 512MB / 4KB

#define MiB(x) ((x) * 1024 * 1024)

// hardcoded for now but maybe can use mailbox
#define TOTAL_MEM_MB  512
#define GPU_MEM_MB    64
#define RAM_MEM_MB    (TOTAL_MEM_MB - GPU_MEM_MB)
#define PERI_MEM_MB   16

#define TOTAL_MEM     MiB(TOTAL_MEM_MB)
#define GPU_MEM       MiB(GPU_MEM_MB)
#define RAM_MEM       MiB(RAM_MEM_MB)
#define PERI_MEM      MiB(PERI_MEM_MB)

#define PERI_PBASE    0x20000000


#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))

#define SECTION_ALIGN_UP(x) (((x) + SECTION_SIZE - 1) & ~(SECTION_SIZE - 1))
#define SECTION_ALIGN_DOWN(x) ((x) & ~(SECTION_SIZE - 1))

typedef struct {
    uintptr_t start;
    uint32_t size;
} PhysMemRegion;

typedef struct {
    PhysMemRegion regions[PMEM_MAX_REGIONS];
    uint32_t count;
} PhysMem;

void phys_mem_init();
PhysMem* phys_mem_get();

#endif
