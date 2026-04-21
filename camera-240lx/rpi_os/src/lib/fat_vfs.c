#include "fat_vfs.h"
#include "fat.h"
#include "lib.h"
#include "kmem.h"

#include <stddef.h>

static int fat32_read(File* f, void* buf, uint32_t n) {
    if (!f || !f->vnode || !f->vnode->data) return 0;

    FatFileInfo* info = (FatFileInfo*) f->vnode->data;
    if (f->offset >= info->filesize) return 0;

    if (f->offset + n > info->filesize) {
        n = info->filesize - f->offset;
    }

    uint8_t* temp_buf;
    fat_read_file_cluster(info->start_cluster, &temp_buf);

    memcpy(buf, temp_buf + f->offset, n);
    kfree(temp_buf);

    printk("read:\n");
    for (int i=0; i < n; i++) {
        printk("%c", ((char*)buf)[i]);
    }
    printk("\n");

    f->offset += n;
    return n;
}
static int fat32_write(File* f, const void* buf, uint32_t n) {
    panic("Unimplemented");
    return 0;
}
static int fat32_close(File* f) {
    if (f->vnode->data) kfree(f->vnode->data);
    if (f->vnode) kfree(f->vnode);
    kfree(f);
    return 0;
}

FileOps fat32_ops = {
    .read = fat32_read,
    .write = fat32_write,
    .ioctl = NULL,
    .close = fat32_close
};
