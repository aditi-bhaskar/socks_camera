#include "vma.h"
#include "vm.h"
#include "process.h"
#include "kmem.h"

#include <stddef.h>

bool vma_check_collision(VMArea* head, uint32_t start, uint32_t n) {
    uint32_t end = start + n;
    for (; head; head = head->next) {
        if (start < head->vm_end && end > head->vm_start) {
            return true;
        }
    }
    return false;
}
uint32_t vma_find_gap(VMArea* head, uint32_t n) {
    uint32_t addr = PAGE_ALIGN_UP(current_process->prog_break);

    while (head) {
        if (head->vm_end <= addr) {
            head = head->next;
            continue;
        }

        if (head->vm_start > addr && head->vm_start - addr >= n) {
            return addr;
        }

        addr = head->vm_end;
        head = head->next;
    }

    if (addr + n >= KERNEL_VBASE) {
        return 0;
    }

    return addr;
}
void vma_insert(VMArea** head, VMArea* vma) {
    VMArea* cur = *head;
    if (!cur || cur->vm_start > vma->vm_start) {
        vma->next = cur;
        *head = vma;
        return;
    }

    for (; cur->next && cur->next->vm_start < vma->vm_start; cur = cur->next);
    vma->next = cur->next;
    cur->next = vma;
}
void vma_remove(VMArea** head, uint32_t start, uint32_t n) {
    uint32_t end = start + n;

    VMArea* cur = *head;
    VMArea* prv = NULL;
    while (cur) {
        if (cur->vm_end > start || cur->vm_start < end) {
            // completely overlap
            if (cur->vm_start >= start && cur->vm_end <= end) {
                if (prv) prv->next = cur->next;
                else *head = cur->next;
                
                VMArea* del = cur;
                cur = cur->next;
                kfree(del);
                continue;
            }

            // intersect midde
            if (cur->vm_start < start && cur->vm_end > end) {
                VMArea* tail = (VMArea*) kmalloc(sizeof(VMArea));
                tail->vm_start = end;
                tail->vm_end = cur->vm_end;
                tail->flags = cur->flags;
                tail->next = cur->next;

                cur->next = tail;
                cur->vm_end = start; 
                
                cur = tail->next;
                continue;
            }

            // trim
            if (cur->vm_start >= start) {
                cur->vm_start = end;
            } else if (cur->vm_end <= end) {
                cur->vm_end = start;
            }
        }

        prv = cur;
        cur = cur->next;
    }
}
