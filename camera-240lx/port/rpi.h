// port/rpi.h — libpi compatibility shim for the rpi_os build.
//
// bt.c / circular.h / gpio-high.h were written against libpi (`rpi.h`). This
// header lets them compile + link under rpi_os instead, by re-declaring the
// few rpi_os primitives they need and providing small shims for the libpi-only
// API (1-arg assert, variadic panic, dev_barrier, delay_ms, kmalloc, ...).
//
// Only the rpi_os "aditi" build puts port/ on the include path, so this never
// shadows the real libpi rpi.h in christy's libpi build.
#ifndef PORT_RPI_H
#define PORT_RPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>   // memcpy / memset / memcmp (size_t-typed; matches bt.c)

// ---- rpi_os primitives we rely on (decls match rpi_os/include/lib.h ABI) ----
void     PUT32(uint32_t addr, uint32_t val);
uint32_t GET32(uint32_t addr);
void     printk(const char *fmt, ...);
void     rpi_reboot(void);
void     mem_barrier_dsb(void);
void     mem_barrier_dmb(void);

// ---- peripheral addressing under rpi_os MMU --------------------------------
// rpi_os maps peripherals at (phys | KERNEL_VBASE). Raw physical addresses are
// unmapped once mmu_enable_caches() runs, so any direct register access must
// OR in the kernel virtual base.
#define PORT_KERNEL_VBASE 0xC0000000u
#define PORT_PERI(phys)   (((uint32_t)(phys)) | PORT_KERNEL_VBASE)

// ---- libpi-style assert / panic --------------------------------------------
// libpi uses 1-arg assert and printf-style panic; rpi_os uses different
// signatures, so define our own here (these win inside any TU that includes
// this shim).
#define panic(...) do {                                                     \
        printk("PANIC %s:%d: ", __FILE__, __LINE__);                        \
        printk(__VA_ARGS__);                                                 \
        printk("\n");                                                        \
        rpi_reboot();                                                        \
    } while (0)

#define assert(x) do {                                                      \
        if (!(x)) panic("assertion failed: %s", #x);                        \
    } while (0)

// ---- barriers / delays / misc ----------------------------------------------
static inline void dev_barrier(void) { mem_barrier_dsb(); }

// busy-spin ~n loop iterations (used by gpio pud sequencing, not timing-critical)
static inline void delay_cycles(unsigned n) {
    for (volatile unsigned i = 0; i < n; i++) { }
}
static inline void rpi_wait(void) { delay_cycles(8); }

// millisecond / microsecond delays -> rpi_os system timer
void sys_timer_delay_ms(uint32_t ms);
void sys_timer_delay_us(uint32_t us);
static inline void delay_ms(uint32_t ms) { sys_timer_delay_ms(ms); }
static inline void delay_us(uint32_t us) { sys_timer_delay_us(us); }

// libpi-style microsecond clock (for BT timeouts in bt.c). Maps to the rpi_os
// free-running system timer. Wraps every ~71 min; fine for short timeouts.
uint32_t sys_timer_get_usec(void);
static inline uint32_t timer_get_usec(void) { return sys_timer_get_usec(); }

static inline void clean_reboot(void) { rpi_reboot(); }

#define debug(...) printk(__VA_ARGS__)

// ---- gpio function selects (libpi gpio.h values) ---------------------------
typedef enum {
    GPIO_FUNC_INPUT  = 0,
    GPIO_FUNC_OUTPUT = 1,
    GPIO_FUNC_ALT0   = 4,
    GPIO_FUNC_ALT1   = 5,
    GPIO_FUNC_ALT2   = 6,
    GPIO_FUNC_ALT3   = 7,
    GPIO_FUNC_ALT4   = 3,
    GPIO_FUNC_ALT5   = 2,
} gpio_func_t;

// ---- kmalloc shim ----------------------------------------------------------
// bt.c uses kmalloc only to allocate fixed HCI packet structs that get pushed
// (by pointer) into the circular queues and consumed immediately in the same
// (polling) call. A small round-robin pool of max-size slots is therefore
// safe and bounded — equivalent in spirit to libpi's never-freed bump pool,
// but without unbounded growth across many image sends.
#define PORT_KMALLOC_SLOT   1056   // >= sizeof(largest HCI packet struct)
#define PORT_KMALLOC_SLOTS  16

static inline void *kmalloc_notzero(unsigned size) {
    static uint8_t  pool[PORT_KMALLOC_SLOTS][PORT_KMALLOC_SLOT]
        __attribute__((aligned(8)));
    static unsigned next = 0;
    if (size > PORT_KMALLOC_SLOT)
        panic("kmalloc size %d > slot %d", size, PORT_KMALLOC_SLOT);
    void *p = pool[next];
    next = (next + 1) % PORT_KMALLOC_SLOTS;
    return p;
}
static inline void *kmalloc(unsigned size) {
    void *p = kmalloc_notzero(size);
    memset(p, 0, size);
    return p;
}
static inline void  kmalloc_init(void) { }
static inline void  kmalloc_init_set_start(void *start) { (void)start; }

#endif // PORT_RPI_H
