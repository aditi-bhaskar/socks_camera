#include "syscall.h"
#include "lib.h"
#include "uart.h"
#include "fd.h"
#include "fat.h"
#include "fat_vfs.h"
#include "process.h"
#include "page_alloc.h"
#include "kmem.h"

#include <stddef.h>

#define check_fd(fd) ((fd >= 0 && fd < MAX_FDS && current_process->fd_table) ? \
        current_process->fd_table[fd] : \
        NULL)
#define file_has_op(f, op) (f && f->vnode && f->vnode->ops && f->vnode->ops->op)
#define file_get_rd_flag(f) (f->flags & O_ACCMODE)

static int sys_write(int fd, const void* buf, int len) {
    File* f = check_fd(fd);
    return (file_has_op(f, write) && (file_get_rd_flag(f) != O_RDONLY)) ?
        f->vnode->ops->write(f, buf, len) :
        -EBADF;
}
static int sys_writev(int fd, const struct iovec* iov, int iovcnt) {
    int total = 0; for (int i = 0; i < iovcnt; i++) {
        total += sys_write(fd, iov[i].iov_base, iov[i].iov_len);
    }
    return total;
}
static int sys_read(int fd, void* buf, int len) {
    File* f = check_fd(fd);
    return (file_has_op(f, read) && (file_get_rd_flag(f) != O_WRONLY)) ?
        f->vnode->ops->read(f, buf, len) :
        -EBADF;
}
static int sys_ioctl(int fd, int req, void* arg) {
    File* f = check_fd(fd);
    return file_has_op(f, ioctl) ?
        f->vnode->ops->ioctl(f, req, arg) :
        -EBADF;
}
static int sys_open(const char* filename, int flags, int mode) {
    int fd = -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (current_process->fd_table[i] == NULL) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -EMFILE;
    }

    uint32_t filesize = 0;
    uint32_t cluster = fat_get_file_cluster(filename, &filesize);
    if (cluster == 0) {
        return -ENOENT;
    }

    FatFileInfo* info = (FatFileInfo*) kmalloc(sizeof(FatFileInfo));
    info->start_cluster = cluster;
    info->filesize = filesize;

    VNode* vnode = (VNode*) kmalloc(sizeof(VNode));
    vnode->ops = &fat32_ops;
    vnode->data = info;

    File* f = (File*) kmalloc(sizeof(File));
    f->vnode = vnode;
    f->offset = 0;
    f->flags = flags;
    f->ref_count = 1;

    current_process->fd_table[fd] = f;
    printk("opened file with fd=%d\n", fd);
    return fd;
}
static int sys_close(int fd) {
    File* f = check_fd(fd);
    if (file_has_op(f, close)) {
        f->vnode->ops->close(f);
    }

    current_process->fd_table[fd] = NULL;
    return 0;
}
static int sys_exit(int code) {
    printk("Process %d exited with code %d\n", current_process->pid, code);

    proc_destroy(current_process);
    current_process = NULL;

    rpi_reset();
    return 0;
}
static int sys_mmap2(void* addr, uint32_t n, int prot, int flags) {
    printk("mmap with %x, %d, %b, %b\n", addr, n, prot, flags);
    uint32_t alloc_start = (uint32_t) addr;
    n = PAGE_ALIGN_UP(n);

    if (flags & MAP_FIXED) {
        if (alloc_start & (PAGE_SIZE - 1)) {
            return -EINVAL;
        }
        if (vma_check_collision(current_process->vma_list, alloc_start, n)) {
            return -EEXIST;
        }
    } else {
        alloc_start = vma_find_gap(current_process->vma_list, n);
        if (!alloc_start) {
            return -ENOMEM;
        }
    }

    // eager loading for now?
    // TODO: demand w/ interrupt
    uint32_t alloc_end = alloc_start + n;
    uint32_t* l1_pt = (uint32_t*) __va(current_process->l1_pt_paddr);
    for (uint32_t va = alloc_start; va < alloc_end; va += PAGE_SIZE) {
        Page* page = page_alloc(0);
        if (!page) {
            return -ENOMEM;
        }

        memset((void*) page_vaddr(page), 0, PAGE_SIZE);
        map_page_4k(l1_pt, va, page_paddr(page), 1);
    }

    VMArea* vma = (VMArea*) kmalloc(sizeof(VMArea));
    vma->vm_start = alloc_start;
    vma->vm_end = alloc_end;
    vma->flags = prot;
    vma->next = NULL;

    printk("new VMA: %x to %x with %b\n", alloc_start, alloc_end, vma->flags);

    vma_insert(&current_process->vma_list, vma);
    return alloc_start;
}
static int sys_unmap(void* addr, uint32_t n) {
    uint32_t unmap_start = (uint32_t) addr;
    if (unmap_start & (PAGE_SIZE - 1)) {
        return -EINVAL;
    }

    n = PAGE_ALIGN_UP(n);
    uint32_t unmap_end = unmap_start + n;
    printk("unmap %x to %x\n", unmap_start, unmap_end);

    uint32_t* l1_pt = (uint32_t*) __va(current_process->l1_pt_paddr);
    for (uint32_t va = unmap_start; va <= unmap_end; va += PAGE_SIZE) {
        uint32_t paddr = unmap_page_4k(l1_pt, va);
        page_free(page_get_p(paddr), 0);
    }

    vma_remove(&current_process->vma_list, unmap_start, n);
    return 0;
}

static int arm_nr_set_tls(uint32_t tls_ptr) {
    // cp15, r13, c0, 3 = user RO thread ID reg
    asm volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(tls_ptr));
    return 0;
}

// called by boot.S SWI interrupt
void swi_syscall_handler(TrapFrame* regs) {
    uint32_t syscall_num = regs->r[7];
    printk("syscall with num %d\n", syscall_num);

    switch (syscall_num) {
        case LINUX_SYS_WRITE:
            regs->r[0] = sys_write(regs->r[0], (const void*) regs->r[1], regs->r[2]);
            break;
        case LINUX_SYS_WRITEV:
            regs->r[0] = sys_writev(regs->r[0],
                    (const struct iovec*) regs->r[1],
                    regs->r[2]);
            break;
        case LINUX_SYS_READ:
            regs->r[0] = sys_read(regs->r[0], (void*) regs->r[1], regs->r[2]);
            break;
        case LINUX_SYS_OPEN:
            regs->r[0] = sys_open((const char*) regs->r[0], regs->r[1], regs->r[2]);
            break;
        case LINUX_SYS_CLOSE:
            regs->r[0] = sys_close(regs->r[0]);
            break;
        case LINUX_SYS_IOCTL :
            regs->r[0] = sys_ioctl(regs->r[0], regs->r[1], (void*) regs->r[2]);
            break;
        case LINUX_SYS_EXIT_GROUP:
        case LINUX_SYS_EXIT:
            regs->r[0] = sys_exit(regs->r[0]);
            break;
        case LINUX_SYS_BRK:
            regs->r[0] = current_process->prog_break; // same value every time, force mmap
            break;
        case LINUX_SYS_MMAP2:
            regs->r[0] = sys_mmap2((void*) regs->r[0], regs->r[1], regs->r[2], regs->r[3]);
            break;
        case LINUX_SYS_UNMAP:
            regs->r[0] = sys_unmap((void*) regs->r[0], regs->r[1]);
            break;
        case LINUX_SYS_SET_TID_ADDR:
            regs->r[0] = current_process->pid; // current thread ID
            break;
        case ARM_NR_set_tls:
            regs->r[0] = arm_nr_set_tls(regs->r[0]);
            break;
        case 125:
            regs->r[0] = 0;
            break;
        default:
            printk("WARNING: Unimplemented system call: %d\n", syscall_num);
            regs->r[0] = -ENOSYS;
            break;
    }
}
