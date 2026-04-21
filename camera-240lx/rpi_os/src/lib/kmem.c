#include "page_alloc.h"
#include "vm.h"
#include "kmem.h"
#include "lib.h"

#include <stddef.h>

// #define DEBUG

static KMemCache kmem_caches[KMEM_NUM_CACHES];
static const uint32_t kmem_cache_size[KMEM_NUM_CACHES] = {
    32,
    64,
    128,
    256,
    512,
    1024,
    2048
};

void kmem_init() {
    for (int i = 0; i < KMEM_NUM_CACHES; i++) {
        kmem_caches[i].size = kmem_cache_size[i];
        ll_init(&kmem_caches[i].slabs_free);
        ll_init(&kmem_caches[i].slabs_partial);
        ll_init(&kmem_caches[i].slabs_full);
    }
}

static int find_size(uint32_t size) {
    int idx = -1;
    for (int i = 0; i < KMEM_NUM_CACHES; i++) {
        if (kmem_cache_size[i] >= size) {
            idx = i;
            break;
        }
    }
    return idx;
}
static Page* format_slab(KMemCache* cache) {
    uint32_t size = cache->size;

    Page* page = page_alloc(0);
    void* new_slab_vaddr = page_vaddr(page);

    uint32_t num_objs = PAGE_SIZE / size;

#ifdef DEBUG
    printk("alloc'ed new slab with %d objs\n", num_objs);
    printk("vaddr: %x\n", new_slab_vaddr);
#endif

    page->flags |= (1 << PAGE_SLAB);

    ll_init(&page->ll);
    page->slab.used = 0;
    page->slab.cache = cache;
    page->slab.free_list = (void*) new_slab_vaddr;

    void* cur = page->slab.free_list;
    for (uint32_t i = 1; i < num_objs; i++) {
        *((void**) cur) = (void*) (new_slab_vaddr + i * size);
        cur = *((void**) cur);
    }
    *((void**) cur) = NULL;

    return page;
}
void* kmalloc(uint32_t size) {
    int idx = find_size(size);
    if (idx == -1) {
        uint8_t order = 0;
        uint32_t sz = PAGE_SIZE;

        while (sz < size) {
            sz *= 2;
            order++;
        }

        Page* p = page_alloc(order);
        return (p ? page_vaddr(p) : NULL);
    }

    KMemCache* cache = &kmem_caches[idx];

    if (ll_empty(&cache->slabs_partial) && ll_empty(&cache->slabs_free)) {
        Page* new_page = format_slab(cache);
        ll_insert(&new_page->ll, &cache->slabs_partial);
    } else if (ll_empty(&cache->slabs_partial)) {
        ll_insert(ll_remove(cache->slabs_free.next), &cache->slabs_partial);
    }

    LList* partial_ll = cache->slabs_partial.next;
    assert(partial_ll, "No partial slabs available");

    Page* page = container_of(partial_ll, Page, ll);

    void* alloc_chunk = page->slab.free_list;
    page->slab.free_list = *((void**) alloc_chunk);
    page->slab.used++;

    if (!page->slab.free_list) {
        ll_insert(ll_remove(partial_ll), &cache->slabs_full);
    }

    return alloc_chunk;
}
void kfree(void* ptr) {
    if (!ptr) return;

    Page* page = page_get((void*) (((uintptr_t) ptr) & ~(PAGE_SIZE - 1)));
    if (!((page->flags >> PAGE_SLAB) & 1)) {
        page_free(page, page->buddy.order);
        return;
    }

    assert(page->slab.used, "Page is empty");

    KMemCache* cache = page->slab.cache;

    // prepend to free list
    *((void**) ptr) = page->slab.free_list;
    page->slab.free_list = ptr;

    if (page->slab.used-- == (uint16_t) (PAGE_SIZE / cache->size)) {
        ll_insert(ll_remove(&page->ll), &cache->slabs_partial);
    } else if (!page->slab.used) {
        ll_insert(ll_remove(&page->ll), &cache->slabs_free);
    }
}
uint32_t kmem_shrink_cache(KMemCache* cache) {
    uint32_t freed = 0;
    while (!ll_empty(&cache->slabs_free)) {
        LList* free_ll = ll_remove(cache->slabs_free.next);
        Page* page = container_of(free_ll, Page, ll);

        page->flags &= ~(1 << PAGE_SLAB);

        page_free(page, 0);
        freed++;
    }

    return freed;
}
uint32_t kmem_shrink_caches_all() {
    uint32_t freed = 0;
    for (int i = 0; i < KMEM_NUM_CACHES; i++) {
        freed += kmem_shrink_cache(&kmem_caches[i]);
    }
    return freed;
}
