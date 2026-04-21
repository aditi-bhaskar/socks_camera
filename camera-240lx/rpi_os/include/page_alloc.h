#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#include "page.h"
#include "phys_mem.h"

void page_alloc_init(PhysMem* pmem);
Page* page_alloc(uint8_t order);
void page_free(Page* page, uint8_t order);

void* page_vaddr(Page* page);
uint32_t page_paddr(Page* page);
Page* page_get(void* vaddr);
Page* page_get_p(uint32_t paddr);

#endif

