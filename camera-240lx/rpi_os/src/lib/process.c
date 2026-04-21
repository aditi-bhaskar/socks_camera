#include "process.h"
#include "vm.h"
#include "kmem.h"
#include "page_alloc.h"
#include "lib.h"
#include "fat.h"
#include "elf.h"
#include "kuser.h"
#include "fd.h"
#include "armv6_asm.h"

#include "sys_timer.h"

#include <stddef.h>

static uint32_t cur_pid = 1;
Process* current_process;

Process* proc_create(const char* filename) {
    Process* p = (Process*) kmalloc(sizeof(Process));
    memset(p, 0, sizeof(Process));

    p->pid = cur_pid++;
    p->state = PROC_READY;

    Page* l1_pt = page_alloc(2);
    p->l1_pt_paddr = page_paddr(l1_pt);

    volatile uint32_t* l1_pt_vaddr = (volatile uint32_t*) page_vaddr(l1_pt);
    memset((void*) l1_pt_vaddr, 0, L1_NUM_PAGES * sizeof(uint32_t));

    for (int i = (KERNEL_VBASE >> 20); i < L1_NUM_PAGES; i++) {
        if (i != (KUSER_PAGE >> 20)) {
            l1_pt_vaddr[i] = l1_page_table[i];
        }
    }

    p->prog_break = PROC_HEAP_START;

    // map STDIN/STDOUT
    for (int i = 0; i < 3; i++) {
        File* f = (File*) kmalloc(sizeof(File));
        f->vnode = &console_vnode;
        f->offset = 0;
        f->flags = O_RDWR; 
        f->ref_count = 1;
        p->fd_table[i] = f;
    }

    // map kuser page (isn't doing anything rn??)
    map_page_4k(l1_pt_vaddr, KUSER_PAGE, global_kuser_paddr, 1);

    // map stack
    Page* stack = page_alloc(0);
    memset(page_vaddr(stack), 0, PAGE_SIZE);
    map_page_4k(l1_pt_vaddr, PROC_STACK_START, page_paddr(stack), 1);
    uint32_t* sp = (uint32_t*) (page_vaddr(stack) + PAGE_SIZE);

    *(--sp) = V6L; // "v6l" for ARMv6
    uint32_t platform_ptr = PROC_STACK_START - (uint32_t) page_vaddr(stack) + (uint32_t) sp;

    *(--sp) = 0;
    *(--sp) = AT_NULL;
    *(--sp) = platform_ptr;
    *(--sp) = AT_PLATFORM;
    *(--sp) = HWCAP_TLS;
    *(--sp) = AT_HWCAP;
    *(--sp) = PAGE_SIZE;
    *(--sp) = AT_PAGESZ;
    *(--sp) = 0; // End of Envp
    *(--sp) = 0; // argv[0] = NULL
    *(--sp) = 0; // argc = 0

    p->context.sp = PROC_STACK_START - (uint32_t) page_vaddr(stack) + (uint32_t) sp;

    // map code
    uint8_t* file_data;
    uint32_t filesize;

    uint32_t t = sys_timer_get_usec();
    fat_read_file(filename, &file_data, &filesize);
    printk("read file in %d usec\n", sys_timer_get_usec() - t);

    Elf32_Ehdr* header = (Elf32_Ehdr*) file_data;
    if (header->e_magic != ELF_MAGIC) {
        panic("Not a valid ELF header!");
    }

    Elf32_Phdr* phdrs = (Elf32_Phdr*) (file_data + header->e_phoff);
    for (uint32_t i = 0; i < header->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uint32_t vaddr  = phdrs[i].p_vaddr;
        uint32_t memsz  = phdrs[i].p_memsz;
        uint32_t filesz = phdrs[i].p_filesz;
        uint32_t offset = phdrs[i].p_offset;

        // align to page
        uint32_t page_start = PAGE_ALIGN_DOWN(vaddr);
        uint32_t page_end = PAGE_ALIGN_UP(vaddr + memsz);

        for (uint32_t va = page_start; va < page_end; va += PAGE_SIZE) {
            Page* page = page_alloc(0);
            void* k_vaddr = page_vaddr(page);
            memset(k_vaddr, 0, PAGE_SIZE);

            map_page_4k(l1_pt_vaddr, va, page_paddr(page), 1);

            uint32_t copy_start = (va > vaddr) ? va : vaddr;
            uint32_t copy_end = ((va + PAGE_SIZE) < (vaddr + filesz)) ?
                (va + PAGE_SIZE) :
                (vaddr + filesz);

            if (copy_start < copy_end) {
                uint32_t chunk_size = copy_end - copy_start;
                uint32_t chunk_file_offset = offset + (copy_start - vaddr);
                uint32_t chunk_page_offset = copy_start - va;

                memcpy((uint8_t*) k_vaddr + chunk_page_offset,
                        file_data + chunk_file_offset,
                        chunk_size);
            }
        }
    }

    kfree(file_data);

    p->context.pc = header->e_entry;
    p->context.cpsr = USER_MODE;

    // TODO: set p->prog_break to end of bss?

    return p;
}
void proc_destroy(Process* p) {
    volatile uint32_t* l1_pt = page_vaddr(page_get_p(p->l1_pt_paddr));

    uint32_t kernel_begin = (KERNEL_VBASE >> 20);
    for (uint32_t i = 0; i < kernel_begin; i++) {
        // is L2 entry
        if ((l1_pt[i] & 3) == 0b01) {
            uint32_t l2_paddr = l1_pt[i] & 0xFFFFFC00;
            volatile uint32_t* l2_pt = (volatile uint32_t*) __va(l2_paddr);

            for (int j = 0; j < L2_NUM_PAGES; j++) {
                if ((l2_pt[j] >> 1) & 1) {
                    page_free(page_get_p(l2_pt[j] & 0xFFFFF000), 0);
                }
            }
            
            page_free(page_get_p(l2_paddr), 0);
        }
    }

    page_free(page_get_p(p->l1_pt_paddr), 2);

    p->state = PROC_DEAD;
    // kfree(p);
}

