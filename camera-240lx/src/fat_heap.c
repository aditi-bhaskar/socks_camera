#include "rpi.h"
#include "fat_heap.h"

// the whole FAT32 driver allocates from here (fat_compat.h does
// `#define kmalloc fat_kmalloc`): the in-RAM FAT (several MB), every directory
// read, AND the per-capture QOI scratch. so this must hold the card's entire
// FAT plus scratch -- ~8MB for a 16GB card. bump it if fat_kmalloc panics.
#define FAT_HEAP_BYTES (16u * 1024 * 1024)

// MUST be its own static storage, NOT &__heap_start__. rpi_os maps all RAM and
// its page allocator (the pool the BT "port" stack allocates from) owns memory
// from __heap_start__ up. a heap based at __heap_start__ hands out the SAME RAM
// the port pool does, so buffers got clobbered mid-save -- files came out the
// right size but all zeros. a static array lives below __heap_start__ (in BSS),
// which the page allocator never touches, so it can't collide.
static uint8_t  fat_heap_mem[FAT_HEAP_BYTES];
static unsigned heap_pos;

void fat_heap_init(void) {
    heap_pos = 0;
}

void *fat_kmalloc(unsigned nbytes) {
    unsigned a = (nbytes + 63u) & ~63u;   // 64-byte align
    if (heap_pos + a > FAT_HEAP_BYTES)
        panic("fat_kmalloc: out of heap (need %d, used %d, cap %d) -- "
              "increase FAT_HEAP_BYTES\n", (int)nbytes, (int)heap_pos, (int)FAT_HEAP_BYTES);
    void *p = fat_heap_mem + heap_pos;
    heap_pos += a;
    memset(p, 0, a);
    return p;
}

unsigned fat_heap_mark(void)            { return heap_pos; }
void     fat_heap_rewind(unsigned mark) { heap_pos = mark; }
unsigned fat_heap_used(void)            { return heap_pos; }
