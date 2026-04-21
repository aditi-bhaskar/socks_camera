#ifndef FD_H
#define FD_H

#include <stdint.h>

typedef struct File File;

typedef struct {
    int (*read)(File* file, void* buf, uint32_t len);
    int (*write)(File* file, const void* buf, uint32_t len);
    int (*ioctl)(File* file, int req, void* arg);
    int (*close)(File* file);
} FileOps;

typedef struct {
    FileOps* ops;
    void* data;
} VNode;

struct File {
    VNode* vnode;
    uint32_t offset;
    uint32_t flags;
    uint32_t ref_count;
};

enum {
    ENOENT = 2,  // no such file/directory
    EBADF  = 9,  // bad file
    ENOMEM = 12, // can't allocate memory
    EEXIST = 17, // file exists
    EINVAL = 21, // invalid arg
    EMFILE = 24, // too many files
    ENOTTY = 25, // not TTY
    ENOSYS = 38,
};

enum {
    STDOUT = 1,
    STDERR = 2
};

enum {
    O_RDONLY,
    O_WRONLY,
    O_RDWR,
    O_ACCMODE,
};

extern VNode console_vnode;

#endif
