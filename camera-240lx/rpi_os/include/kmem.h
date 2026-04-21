#ifndef KMEM_H
#define KMEM_H

#include "linked_list.h"

#include <stdint.h>

// Slab allocator (https://www.kernel.org/doc/gorman/html/understand/understand011.html)
#define KMEM_NUM_CACHES 7

// stores objects within a 4KB page
typedef struct {
    uint32_t size;

    // head nodes (next node is first slab)
    LList slabs_free;
    LList slabs_partial;
    LList slabs_full;
} KMemCache;

void kmem_init();
void* kmalloc(uint32_t size);
void kfree(void* ptr);

// return memory back to OS (returns # of freed pages)
uint32_t kmem_shrink_cache(KMemCache* cache);
uint32_t kmem_shrink_caches_all();

#endif
