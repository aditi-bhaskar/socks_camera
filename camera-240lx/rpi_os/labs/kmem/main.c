#include "uart.h"
#include "lib.h"
#include "page_alloc.h"
#include "vm.h"
#include "kmem.h"

#define DEBUG
#include "debug.h"

void main() {
    uint32_t sp;
    asm volatile ("mov %0, sp" :: "r"(sp));
    printk("main sp: %x, %b, %d\n", sp, sp, sp);

    // uint32_t addr = 0x21000000;
    // GET32(addr);

    /*
    void* pg1 = page_alloc(0);
    void* pg2 = page_alloc(0);
    printk("pg1: %x, pg2: %x\n", (uint32_t) pg1, (uint32_t) pg2);

    uint32_t paddr = 0x00100000, vaddr = 0x00200000;
    map_page_4k(paddr, paddr);

    PUT32(paddr, 0xdeafbeef);
    printk("before map: %x\n", GET32(paddr));

    map_page_4k(vaddr, paddr);

    PUT32(vaddr, 0xFEEDFACE);
    printk("after map: %x\n", GET32(paddr));
    */

    // printk("%x\n", GET32(0x00BFF800));
    // printk("%x\n", GET32(0x00C00000));


    // test getting multiple pages
    void* page1 = page_alloc(1);
    void* page2 = page_alloc(2);
    void* page3 = page_alloc(3);
    void* page4 = page_alloc(4);
    printk("1: %x\n2: %x\n3: %x\n4: %x\n", page1, page2, page3, page4);

    void* page8 = page_alloc(8);
    void* page9 = page_alloc(9);
    void* page10 = page_alloc(10);
    void* page8_2 = page_alloc(8);
    printk("1MB: %x\n1MB: %x\n2MB: %x\n3MB: %x\n", page8, page8_2, page9, page10);

    // commenting any one of these should cause OOM
    page_free(page1, 1);
    page_free(page2, 2);
    page_free(page3, 3);
    page_free(page4, 4);
    page_free(page8, 8);
    page_free(page9, 9);
    page_free(page10, 10);
    page_free(page8_2, 8);

    // /*
    const int N = 10;
    void* addr[N];
    // /*
    for (int i = 0; i < 227326; i++) {
        void* v = kmalloc(2048);

        if (i < N) {
            addr[i] = v;
            printk("%x\n", addr[i]);
        }

        if (i >= 227323) {
            printk("%d\n", i);
        }
    }
    // */
    /*
    for (int i = 0; i < 113663; i++) {
        void* v = kmalloc(4096);

        if (i < N) {
            addr[i] = v;
            printk("%x\n", addr[i]);
        }
    }
    */
    kfree(addr[0]);
    kfree(addr[1]);
    printk("freeing caches: %d\n", kmem_shrink_caches_all());

    void* addr1 = kmalloc(32);
    printk("32: %x\n", addr1);

    void* addr2 = kmalloc(32);
    printk("32: %x\n", addr2);

    void* addr3 = kmalloc(128);
    printk("128: %x\n", addr3);

    void* addr4 = kmalloc(128);
    printk("128: %x\n", addr4);

    for (int i = 2; i < N; i++) {
        kfree(addr[i]);
    }
    printk("freeing caches: %d\n", kmem_shrink_caches_all());

    void* addr5 = kmalloc(2048);
    printk("2048: %x\n", addr5);

    void* addr6 = kmalloc(2048);
    printk("2048: %x\n", addr6);

    void* addr7 = kmalloc(2048);
    printk("2048: %x\n", addr7);

    kfree(addr5);
    kfree(addr6);

    void* addr8 = kmalloc(2048);
    printk("2048: %x\n", addr8);

    void* addr9 = kmalloc(2048);
    printk("2048: %x\n", addr9);

    void* addr10 = kmalloc(2048);
    printk("2048: %x\n", addr10);

    void* addr11 = kmalloc(2048);
    printk("2048: %x\n", addr11);

    extern uint32_t __l1_page_table_start__;
    printk("l1 page table start: %x\n", &__l1_page_table_start__);

    printk("%d", -1);
    // */

    // GET32(0);
}
