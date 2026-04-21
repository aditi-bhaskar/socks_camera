#ifndef FAT_HEAP_H
#define FAT_HEAP_H

// Dedicated bump allocator for the ported 140e FAT32 driver.
//
// The aditi build's port/rpi.h kmalloc is a tiny fixed packet pool (for the BT
// stack) and cannot satisfy the FAT32 driver, which loads the entire FAT into
// RAM (can be several MB) plus per-call directory/file buffers. This allocator
// hands out memory from "usable RAM" (the linker's __heap_start__, which sits
// above the kernel image, BSS, and the L1 page table). All 512MB of RAM is
// section-mapped (see rpi_os vm.c), so a multi-MB heap here is safe.
//
// There is no free(); instead use mark()/rewind() to reclaim the per-capture
// scratch (directory reads, dirent copies) after each save, while keeping the
// permanent allocation made by fat32_mk (the in-RAM FAT).

void     fat_heap_init(void);            // call once at startup
void    *fat_kmalloc(unsigned nbytes);   // zeroed, 64-byte aligned
unsigned fat_heap_mark(void);            // current high-water position
void     fat_heap_rewind(unsigned mark); // free everything past `mark`
unsigned fat_heap_used(void);            // bytes currently in use

#endif // FAT_HEAP_H
