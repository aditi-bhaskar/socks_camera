#ifndef FAT_VHS_H
#define FAT_VHS_H

#include "fd.h"

typedef struct {
    uint32_t start_cluster;
    uint32_t filesize;
} FatFileInfo;

extern FileOps fat32_ops;

#endif
