#ifndef SYSCALL_H
#define SYSCALL_H

#include "process.h"

enum {
    LINUX_SYS_EXIT          = 1,
    LINUX_SYS_READ          = 3,
    LINUX_SYS_WRITE         = 4,
    LINUX_SYS_OPEN          = 5,
    LINUX_SYS_CLOSE         = 6,
    LINUX_SYS_BRK           = 45,
    LINUX_SYS_IOCTL         = 54,
    LINUX_SYS_UNMAP         = 91,
    LINUX_SYS_WRITEV        = 146,
    LINUX_SYS_MMAP2         = 192,
    LINUX_SYS_EXIT_GROUP    = 248,
    LINUX_SYS_SET_TID_ADDR  = 256,

    ARM_NR_set_tls          = 0xF0005,
};

struct iovec {
    void* iov_base; // ptr to string chunk
    uint32_t iov_len; // length of chunk
};

#endif
