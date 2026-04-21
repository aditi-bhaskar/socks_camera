#include "lib.h"
#include "fat.h"
#include "process.h"

#define DEBUG
#include "debug.h"

void main() {
    fat_init();

    Process* p = proc_create("TEST4   ELF");
    // Process* p = proc_create("TEST3   ELF");
    if (!p) {
        panic("can't create process\n");
    }

    current_process = p;

    printk("Dropping into User Mode...\n");
    printk("%x\n", p->context.pc);
    proc_run(&p->context, p->l1_pt_paddr);

    panic("shouldn't reach here\n");
}
