#ifndef FAT_COMPAT_H
#define FAT_COMPAT_H

// Compat shim so the ported 140e FAT32 sources build in the aditi
// (rpi_os + port/) environment. Include this FIRST in each fat .c, before its
// own `#include "rpi.h"`. Because rpi.h is pulled in here (defining the static
// inline port kmalloc) and ONLY THEN do we `#define kmalloc fat_kmalloc`, the
// macro rewrites the FAT driver's call sites to the dedicated multi-MB
// fat_heap without touching the port pool the BT stack relies on.

#include "rpi.h"        // types, printk, panic, assert, memcpy/memset/str* (string.h)
#include "fat_heap.h"   // fat_kmalloc

// 140e helper (defined in fat_libc.c): 1 if all n bytes are zero.
int memiszero(const void *p, unsigned n);

#define kmalloc fat_kmalloc

#ifndef trace
#define trace(...)  printk(__VA_ARGS__)
#endif
#ifndef output
#define output(...) printk(__VA_ARGS__)
#endif
#ifndef demand
// 140e's demand() takes a free-form (often unquoted) message, e.g.
//   demand(cnt < 3, weird for cs140e if larger!);
// so we stringize the message rather than treat it as printf args.
#define demand(_cond, ...) do { \
        if (!(_cond)) panic("demand failed: %s\n", #__VA_ARGS__); \
    } while (0)
#endif
#ifndef unimplemented
#define unimplemented() panic("unimplemented\n")
#endif

// 140e stringize helpers (used by mbr_part_str's T() table, etc.)
#ifndef _STRING
#define _STRING(x)  #x
#endif
#ifndef _XSTRING
#define _XSTRING(x) _STRING(x)
#endif

#endif // FAT_COMPAT_H
