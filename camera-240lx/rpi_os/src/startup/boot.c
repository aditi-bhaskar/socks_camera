#include "lib.h"
#include "uart.h"
#include "mmu.h"
#include "vm.h"
#include "phys_mem.h"
#include "page_alloc.h"
#include "kmem.h"
#include "kuser.h"

__attribute__((interrupt("ABORT")))
void abort_handler(uint32_t sp) {
    printk("sp: %x", sp);
    panic("asdf");
}

void cstart() {
    uart_init();

    phys_mem_init();
    page_alloc_init(phys_mem_get());
    kmem_init();
    kuser_init();

    // enable interrupts
    asm volatile ("cpsie i");

    void main();
    main();
    
    rpi_reset();
}
