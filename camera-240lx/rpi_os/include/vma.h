#ifndef VMA_H
#define VMA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct vm_area {
    uint32_t vm_start;
    uint32_t vm_end;
    uint32_t flags;
    struct vm_area* next;
} VMArea;

enum {
    MAP_SHARED      = 0x01,
    MAP_PRIVATE     = 0x02,
    MAP_FIXED       = 0x10,
    MAP_ANONYMOUS   = 0x20,
};

// TODO: vma flags?
// enum {
// };

bool vma_check_collision(VMArea* head, uint32_t start, uint32_t n);
uint32_t vma_find_gap(VMArea* head, uint32_t n);
void vma_insert(VMArea** head, VMArea* vma);
void vma_remove(VMArea** head, uint32_t start, uint32_t n);

#endif

/* need these flags
VM_READ

Pages can be read from

VM_WRITE

Pages can be written to

VM_EXEC

Pages can be executed

VM_SHARED

Pages are shared

VM_MAYREAD

The VM_READ flag can be set

VM_MAYWRITE

The VM_WRITE flag can be set

VM_MAYEXEC

The VM_EXEC flag can be set

VM_MAYSHARE

The VM_SHARE flag can be set

VM_GROWSDOWN

The area can grow downward

VM_GROWSUP

The area can grow upward

VM_SHM

The area is used for shared memory

VM_DENYWRITE

The area maps an unwritable file

VM_EXECUTABLE

The area maps an executable file

VM_LOCKED

The pages in this area are locked

VM_IO

The area maps a device's I/O space

VM_SEQ_READ

The pages seem to be accessed sequentially

VM_RAND_READ

The pages seem to be accessed randomly

VM_DONTCOPY

This area must not be copied on fork()

VM_DONTEXPAND

This area cannot grow via mremap()

VM_RESERVED

This area must not be swapped out

VM_ACCOUNT

This area is an accounted VM object

VM_HUGETLB

This area uses hugetlb pages

VM_NONLINEAR

This area is a nonlinear mapping
*/
