#include "phys_mem.h"
#include "lib.h"
#include "vm.h"

#define DEBUG

void phys_mem_add_region(PhysMem* pmem, uintptr_t rstart, uint32_t rsize) {
    assert(pmem->count < PMEM_MAX_REGIONS, "Exceeded max physical regions");
    pmem->regions[pmem->count++] = (PhysMemRegion) { rstart, rsize };
}
void phys_mem_reserve_region(PhysMem* pmem, uintptr_t rstart, uint32_t rsize) {
    // carve out regions from pmem
    rstart = PAGE_ALIGN_DOWN(rstart);
    uintptr_t rend = PAGE_ALIGN_UP(rstart + rsize);

    PhysMemRegion new_regions[PMEM_MAX_REGIONS];
    uint32_t new_count = 0;

    for (uint32_t i = 0; i < pmem->count; i++) {
        uintptr_t start = pmem->regions[i].start;
        uintptr_t end = start + pmem->regions[i].size;

        // no intersections
        if (rend <= start || rstart >= end) {
            new_regions[new_count++] = pmem->regions[i];
            continue;
        }

        if (rstart > start) { // left intersection 
            new_regions[new_count++] = (PhysMemRegion) { start, rstart - start };
        }
        if (rend < end) { // right intersection 
            new_regions[new_count++] = (PhysMemRegion) { rend, end - rend };
        }
    }

    for (uint32_t i = 0; i < new_count; i++) {
        pmem->regions[i] = new_regions[i];
    }
    pmem->count = new_count;
}


static PhysMem phys_mem;

void phys_mem_init() {
    phys_mem.count = 0;
    phys_mem_add_region(&phys_mem, 0, RAM_MEM);

    extern uint32_t __heap_start__;
    uint32_t reserved_size = SECTION_ALIGN_UP((uint32_t) __pa(&__heap_start__));
    phys_mem_reserve_region(&phys_mem, 0, reserved_size);

#ifdef DEBUG
    printk("total RAM mem (bytes): %d\n", RAM_MEM);
    printk("%d\n", phys_mem.count);
    for (uint32_t i = 0; i < phys_mem.count; i++) {
        printk("i: %d, st: %d, sz: %d\n", i, phys_mem.regions[i].start, phys_mem.regions[i].size);
        printk("beg page: %d, end page: %d\n", (phys_mem.regions[i].start / PAGE_SIZE),
                (phys_mem.regions[i].start + phys_mem.regions[i].size) / PAGE_SIZE);
    }
#endif

}
PhysMem* phys_mem_get() {
    return &phys_mem;
}
