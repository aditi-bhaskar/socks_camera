#ifndef PAGE_H
#define PAGE_H

#include "linked_list.h"

#include <stdint.h>

// max 4MB sections (2^10 * sizeof(Page))
#define MAX_PAGE_ORDER 11

typedef enum {
    PAGE_FREE,
    PAGE_SLAB,
} PageFlags;

typedef struct {
    LList ll;

    union {
        struct {
            uint8_t order;
        } buddy;

        struct {
            void* free_list;
            void* cache;
            uint16_t used;
        } slab;
    };

    uint32_t flags;
} Page;

#endif
