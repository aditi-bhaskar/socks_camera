// this test, we used some amount of AI to write
// we wrote the rest of the fat32 implementation tho

// main_sdtest.c — standalone SD/FAT32 write+readback test across file sizes.
//
// no camera / BT / OLED. mounts the card, then for a sweep of sizes (sub-cluster
// up to a few hundred KB) it writes a TESTnnnn.BIN full of a known pattern,
// reads it back off the card, and checks the bytes survived. each line prints a
// verdict so we can see EXACTLY where writes start failing and whether a failure
// is all-zeros (data never landed) or garbage (wrong location / corruption).
//
// build:  make BUILD=sdtest
// then flash kernel_sdtest.img and watch the UART.

#include "rpi.h"
#include "uart.h"
#include "mmu.h"
#include "pi-sd.h"
#include "mbr.h"
#include "fat32.h"
#include "fat_heap.h"

static fat32_fs_t  g_fs;
static pi_dirent_t g_root;

// "TESTnnnn.BIN" into out[13].
static void make_name(char *out, uint32_t idx) {
    out[0]='T'; out[1]='E'; out[2]='S'; out[3]='T';
    for (int i = 7; i >= 4; i--) { out[i] = '0' + (idx % 10); idx /= 10; }
    out[8]='.'; out[9]='B'; out[10]='I'; out[11]='N'; out[12]=0;
}

// byte pattern that is never 0 (so "all zeros" is unmistakable) and depends on
// both position and file index (so a shifted/wrong-cluster read also stands out).
static uint8_t patt(uint32_t i, uint32_t seed) {
    return (uint8_t)((i + seed * 13u) % 255u + 1u);   // 1..255
}

static void test_size(uint32_t nbytes, uint32_t idx) {
    char name[13];
    make_name(name, idx);
    unsigned mark = fat_heap_mark();

    uint8_t *wbuf = fat_kmalloc(nbytes);
    for (uint32_t i = 0; i < nbytes; i++) wbuf[i] = patt(i, idx);

    // delete any leftover from a previous run so create() won't reject the name.
    fat32_delete(&g_fs, &g_root, name);

    int created = 0, wrote = 0;
    pi_dirent_t *de = fat32_create(&g_fs, &g_root, name, 0);
    created = (de != 0);
    if (created) {
        pi_file_t f = { .data = (char *)wbuf, .n_alloc = nbytes, .n_data = nbytes };
        wrote = fat32_write(&g_fs, &g_root, name, &f);
        fat32_flush(&g_fs);
    }

    const char *verdict = "WRITE-FAIL";
    int first_bad = -1;
    uint32_t got = 0;
    if (wrote) {
        pi_file_t *rf = fat32_read(&g_fs, &g_root, name);
        if (!rf) {
            verdict = "READ-NULL";
        } else {
            got = rf->n_data;
            uint8_t *rb = (uint8_t *)rf->data;
            int allzero = 1;
            for (uint32_t i = 0; i < nbytes; i++)
                if (rb[i]) { allzero = 0; break; }
            for (uint32_t i = 0; i < nbytes; i++)
                if (rb[i] != patt(i, idx)) { first_bad = (int)i; break; }
            if (got != nbytes)      verdict = "SIZE-MISMATCH";
            else if (first_bad < 0) verdict = "OK";
            else if (allzero)       verdict = "ALL-ZERO";
            else                    verdict = "GARBAGE";
        }
    }

    // NOTE: rpi_os printk has no width/justify specifiers (%7d, %-13s) -- it
    // prints them literally AND doesn't consume the arg, shifting everything
    // after. stick to plain %d / %s / %x.
    printk("[sdtest] %s size=%d create=%d write=%d got=%d verdict=%s",
           name, (int)nbytes, created, wrote, (int)got, verdict);
    if (first_bad >= 0) printk(" first_bad@%d", first_bad);
    printk(" (heap peak %d KB)\n", (int)(fat_heap_used() / 1024));

    fat_heap_rewind(mark);
}

void main() {
    uart_init();
    mmu_enable_caches();   // emmc registers are at VBASE, so MMU must be on
    printk("\n[sdtest] SD/FAT32 write+readback test\n");

    fat_heap_init();
    if (!pi_sd_init_try()) { printk("[sdtest] SD init failed -- no card?\n"); return; }

    mbr_t *mbr = mbr_read();
    mbr_partition_ent_t part;
    memcpy(&part, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    if (!mbr_part_is_fat32(part.part_type)) {
        printk("[sdtest] partition 0 is not FAT32\n");
        return;
    }
    g_fs   = fat32_mk(&part);
    g_root = fat32_get_root(&g_fs);

    uint32_t cl = g_fs.sectors_per_cluster * 512;
    printk("[sdtest] mounted. sec/clus=%d  cluster=%d bytes  FAT-in-RAM=%d KB\n",
           g_fs.sectors_per_cluster, (int)cl, (int)(fat_heap_used() / 1024));

    // sweep: sub-cluster, ~1 cluster, and several multi-cluster sizes. the
    // interesting transition is 1 cluster -> 2+ clusters (the chain walk).
    uint32_t sizes[16];
    int ns = 0;
    sizes[ns++] = 256;
    sizes[ns++] = 4096;
    if (cl > 2) sizes[ns++] = cl / 2;
    sizes[ns++] = cl;
    sizes[ns++] = cl + 1;
    sizes[ns++] = 2 * cl;
    sizes[ns++] = 3 * cl;
    sizes[ns++] = 5 * cl;
    sizes[ns++] = 80 * 1024;     // ~ a real QOI capture
    sizes[ns++] = 200 * 1024;

    for (int k = 0; k < ns; k++)
        test_size(sizes[k], (uint32_t)(k + 1));

    printk("[sdtest] done. verdicts above; pull TEST*.BIN to double-check.\n");
}
