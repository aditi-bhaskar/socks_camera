#include "kuser.h"
#include "lib.h"
#include "page_alloc.h"

uint32_t global_kuser_paddr;
void kuser_init() {
    Page* p = page_alloc(0);
    memset(page_vaddr(p), 0, PAGE_SIZE);
    global_kuser_paddr = page_paddr(p);
}
