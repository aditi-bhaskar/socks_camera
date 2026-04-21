// TODO: FAT cache, use DMA

#include "fat.h"
#include "emmc.h"
#include "uart.h"
#include "lib.h"
#include "kmem.h"

#include <stddef.h>

#define DEBUG
#include "debug.h"

// #define VERBOSE

static uint32_t partition_lba;
static uint32_t fat_lba;
static uint32_t data_lba;
static bpb_t* bpb;

static uint32_t fat_cache_sector;
static uint8_t* fat_cache;

static uint32_t fatdir_get_cluster(fatdir_t* dir) {
    return ((uint32_t) dir->ch << 16) | dir->cl;
}
static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_lba + (cluster - 2) * bpb->nsec_per_cluster;
}

int fat_getpartition() {
    uint8_t* mbr = kmalloc(SECTOR_SIZE);
    bpb = (bpb_t*) mbr; // can overwrite since we only read mbr once

    // read the partitioning table
    if (sd_readblock(0, mbr, 1)) {
        // check magic
        if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
            printk("ERROR: Bad magic in MBR\n");
            return 0;
        }

        // check partition type: 0x0B = FAT32 CHS, 0x0C = FAT32 LBA
        if (mbr[0x1C2] != 0xB && mbr[0x1C2] != 0xC) {
            printk("ERROR: Wrong partition type (need FAT32, got %d)\n", mbr[0x1C2]);
            return 0;
        }

        // should be this, but compiler generates bad code...
        partition_lba = mbr[0x1C6] +
            (mbr[0x1C7] << 8)  +
            (mbr[0x1C8] << 16) +
            (mbr[0x1C9] << 24);

        // read the boot record
        if (!sd_readblock(partition_lba, bpb, 1)) {
            printk("ERROR: Unable to read boot record\n");
            kfree(mbr);
            return 0;
        }

        // check file system type. We don't use cluster numbers for that, but magic bytes
        if (!(bpb->fst[0] == 'F' && bpb->fst[1] == 'A' && bpb->fst[2] == 'T') &&
                !(bpb->fst2[0] == 'F' && bpb->fst2[1] == 'A' && bpb->fst2[2] == 'T')) {
            printk("ERROR: Unknown file system type\n");
            return 0;
        }

        fat_lba = partition_lba + bpb->reserved_nsec;
        data_lba = fat_lba + bpb->nfats * bpb->nsec_per_fat;

#ifdef VERBOSE
        printk("FAT Bytes per Sector: %d", bpb->nbytes_per_sec);
        printk("\nFAT Sectors per Cluster: %d", bpb->nsec_per_cluster);
        printk("\nFAT Number of FAT: %d", bpb->nfats);
        printk("\nFAT Sectors per FAT: %d", bpb->nsec_per_fat);
        printk("\nFAT Reserved Sectors Count: %d", bpb->reserved_nsec);
        printk("\nFAT First data sector: %x", data_lba);
        printk("\n");
#endif

        return 1;
    }
    return 0;
}

uint32_t fat_next_cluster(uint32_t cluster) {
    if (!fat_cache) {
        fat_cache = kmalloc(bpb->nbytes_per_sec);
    }

    uint32_t fat_offset_bytes = cluster * sizeof(uint32_t);
    uint32_t target_sector = fat_lba + (fat_offset_bytes / bpb->nbytes_per_sec);
    if (target_sector != fat_cache_sector) {
        sd_readblock(target_sector, fat_cache, 1);
        fat_cache_sector = target_sector;
    }

    uint32_t res = *((uint32_t*) (fat_cache + (fat_offset_bytes % bpb->nbytes_per_sec)));
    return res & 0x0FFFFFFF;
}
uint32_t fat_read_cluster(uint32_t cluster, uint8_t* data) {
    sd_readblock(cluster_to_lba(cluster), data, bpb->nsec_per_cluster);
    return fat_next_cluster(cluster);
}
static uint32_t cluster_chain_len(uint32_t cluster) {
    uint32_t res = 0;
    for (; cluster < LAST_CLUSTER; res++, cluster = fat_next_cluster(cluster));
    return res;
}
static void cluster_chain_read(uint32_t cluster, uint8_t* data) {
    const uint32_t bytes_per_cluster = bpb->nsec_per_cluster * bpb->nbytes_per_sec;
    for (int i = 0; cluster < LAST_CLUSTER; i += bytes_per_cluster) {
        cluster = fat_read_cluster(cluster, &data[i]);
    }
}

fatdir_t* fat_statroot() {
    uint32_t num_clusters = cluster_chain_len(bpb->root_cluster);
    fatdir_t* dir = kmalloc(num_clusters * bpb->nsec_per_cluster * bpb->nbytes_per_sec);
    cluster_chain_read(bpb->root_cluster, (uint8_t*) dir);
    return dir;
}

int fat_skip_dirent(fatdir_t* dir) {
    uint8_t attr = dir->attr[0];
    return (attr & FAT32_HIDDEN) ||
        (attr & FAT32_SYS_FILE) ||
        (attr & FAT32_VOLUME_LBL) ||
        (dir->name[0] == 0xE5);
}
void fat_get_plys(fatdir_t** dirs, uint8_t** lfns, uint32_t* num_files) {
    fatdir_t* dir = fat_statroot();
    const char* ext = "PLY";

    uint32_t dir_cnt = 0;
    for (fatdir_t* d = dir; d->name[0]; d++) {
        if (fat_skip_dirent(d)) {
            continue;
        }

        if (!memcmp(d->ext, ext, 3)) {
            dir_cnt++;
        }
    }

    fatdir_t* res = kmalloc(dir_cnt * sizeof(fatdir_t));
    uint8_t* lfn = kmalloc(dir_cnt * MAX_LFN_LEN);
    memset(lfn, 0, dir_cnt * MAX_LFN_LEN);

    uint8_t cksum;
    for (int i = 0, l = 0; dir->name[0]; dir++) {
        if ((dir->attr[0] & 0xF) == FAT32_LFN) {
            fatdir_lfn_t* lfn_dir = (fatdir_lfn_t*) dir;
            assert(lfn_dir->lfn == 0x0F, "LFN type mismatch");

            cksum = lfn_dir->cksum;

            uint32_t offset = ((lfn_dir->seqno & 0x1F) - 1) * 13;
            for (int j = 0; j < 5; j++) {
                lfn[i * MAX_LFN_LEN + offset + j] = lfn_dir->name0[j] & 0xFF;
            }
            for (int j = 0; j < 6; j++) {
                lfn[i * MAX_LFN_LEN + offset + 5 + j] = lfn_dir->name1[j] & 0xFF;
            }
            for (int j = 0; j < 2; j++) {
                lfn[i * MAX_LFN_LEN + offset + 11 + j] = lfn_dir->name2[j] & 0xFF;
            }

            l += 13;

            continue;
        }
        if (fat_skip_dirent(dir)) {
            while (l > 0) {
                lfn[i * MAX_LFN_LEN + (--l)] = 0;
            }
            continue;
        }

        if (!memcmp(dir->ext, ext, 3)) {
            // check cksum
            if (l) {
                uint8_t ck = 0;
                for (int j = 0; j < 11; j++) {
                    ck = ((ck & 1) ? 0x80 : 0) + (ck >> 1) + *(dir->name + j);
                }
                assert(ck == cksum, "LFN checksum mismatch");
            }

            memcpy(&res[i++], dir, sizeof(fatdir_t));
            l = 0;
        }
    }

    *num_files = dir_cnt;
    *dirs = res;
    *lfns = lfn;
}
uint32_t fat_get_file_cluster(const char* fn, uint32_t* filesize) {
    fatdir_t* dir = fat_statroot();

    for (; dir->name[0]; dir++) {
        if (fat_skip_dirent(dir)) {
            continue;
        }

        if (!memcmp(dir->name, fn, 8) && !memcmp(dir->ext, fn+8, 3)) {
#ifdef VERBOSE
            printk("FAT File %s starts at cluster: %x size: %d\n", fn, fatdir_get_cluster(dir), dir->size);
#endif

            if (filesize) {
                *filesize = dir->size;
            }

            uint32_t res = fatdir_get_cluster(dir);
            kfree(dir);
            return res;
        }
    }

    printk("ERROR: file not found\n");
    kfree(dir);
    return 0;
}

void fat_init() {
    assert(sd_init() == SD_OK, "SD init failed");

#ifdef VERBOSE
    printk("SD init OK\n");
#endif

    assert(fat_getpartition(), "FAT partition not found");

#ifdef VERBOSE
    printk("FAT partition OK\n");
#endif
}

void fat_read_file_cluster(uint32_t cluster, uint8_t** data) {
    uint32_t num_clusters = cluster_chain_len(cluster);
    *data = kmalloc(num_clusters * bpb->nsec_per_cluster * bpb->nbytes_per_sec);
    cluster_chain_read(cluster, *data);
}
void fat_read_file(const char* fn, uint8_t** data, uint32_t* filesize) {
    uint32_t cluster = fat_get_file_cluster(fn, filesize);
    assert(cluster, "File not found");
    fat_read_file_cluster(cluster, data);
}
uint32_t fat_bytes_per_sector() {
    return bpb->nbytes_per_sec;
}
