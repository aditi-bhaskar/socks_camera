#include "page_alloc.h"
#include "lib.h"
#include "vm.h"

#include <stddef.h>

static Page mem_map[NUM_PAGES];
static LList free_pages_head[MAX_PAGE_ORDER];

void page_alloc_init(PhysMem* pmem) {
    for (uint32_t i = 0; i < MAX_PAGE_ORDER; i++) {
        ll_init(&free_pages_head[i]);
    }

    // physical region sections are guaranteed to align to 1MB sections
    uint32_t page_start = pmem->regions[0].start / PAGE_SIZE;
    uint32_t page_end = (pmem->regions[0].start + pmem->regions[0].size) / PAGE_SIZE;

    printk("%d to %d\n", page_start, page_end);
    printk("num pages: %d\n", NUM_PAGES);

    // init all pages in usable RAM (mark valid pages)
    for (uint32_t i = 0; i < NUM_PAGES; i++) {
        if (i >= page_start && i < page_end) {
            mem_map[i].flags = (1 << PAGE_FREE);
            ll_init(&mem_map[i].ll);
        } else {
            mem_map[i].flags = 0;
        }
    }

    // insert into free lists greedily, starting with largest page size
    // address has to align with 2^order
    // using insert_prev so lower addresses are handed out first
    uint32_t idx = page_start;
    while (idx < page_end) {
        uint8_t order = MAX_PAGE_ORDER - 1;
        for (; order > 0; order--) {
            uint32_t bs = (1 << order);
            if ((idx + bs <= page_end) && ((idx & (bs - 1)) == 0)) {
                break;
            }
        }

        mem_map[idx].buddy.order = order;
        ll_insert_prev(&mem_map[idx].ll, &free_pages_head[order]);

        idx += (1 << order);
    }
}

Page* page_alloc(uint8_t order) {
    uint8_t cur_order = order;
    while (cur_order < MAX_PAGE_ORDER && ll_empty(&free_pages_head[cur_order])) {
        cur_order++;
    }

    if (cur_order == MAX_PAGE_ORDER) {
        panic("Page alloc OOM or order too big");
        return NULL; // OOM
    }

    LList* page_ll = ll_remove(free_pages_head[cur_order].next);
    Page* page = container_of(page_ll, Page, ll);

    while (cur_order-- > order) {
        // get buddy page (can never be out of range)
        Page* buddy = &mem_map[(page - mem_map) ^ (1 << cur_order)];

        buddy->flags |= (1 << PAGE_FREE);
        buddy->buddy.order = cur_order;
        ll_insert(ll_remove(&buddy->ll), &free_pages_head[cur_order]);
    }

    page->flags &= ~(1 << PAGE_FREE);
    page->buddy.order = order;
    return page;
}
void page_free(Page* page, uint8_t order) {
    uint32_t page_idx = page - mem_map;

    // coalesce into largest order
    for (; order < MAX_PAGE_ORDER - 1; order++) {
        uint32_t buddy_idx = (page - mem_map) ^ (1 << order);
        if (buddy_idx >= NUM_PAGES) {
            break;
        }

        Page* buddy = &mem_map[buddy_idx];
        if (!(buddy->flags & (1 << PAGE_FREE)) || buddy->buddy.order != order) {
            break;
        }

        ll_remove(&buddy->ll);

        if (page_idx > buddy_idx) {
            page_idx = buddy_idx;
            page = buddy;
        }
    }
    
    page->flags |= (1 << PAGE_FREE);
    page->buddy.order = order;
    ll_insert(&page->ll, &free_pages_head[order]);
}

void* page_vaddr(Page* page) {
    return __va((page - mem_map) * PAGE_SIZE);
}
uint32_t page_paddr(Page* page) {
    return (uint32_t) ((page - mem_map) * PAGE_SIZE);
}
Page* page_get(void* vaddr) {
    assert(((uintptr_t) vaddr) % PAGE_SIZE == 0, "Invalid physical address");
    return &mem_map[__pa(vaddr) / PAGE_SIZE];
}
Page* page_get_p(uint32_t paddr) {
    assert(((uintptr_t) paddr) % PAGE_SIZE == 0, "Invalid physical address");
    return &mem_map[paddr / PAGE_SIZE];
}

