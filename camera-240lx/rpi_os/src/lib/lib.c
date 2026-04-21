#include "lib.h"
#include "uart.h"
#include "arena_allocator.h"
#include "vm.h"

#define PM_RSTC (0x2010001C | KERNEL_VBASE)
#define PM_WDOG (0x20100024 | KERNEL_VBASE)
#define PM_PASSWORD 0x5A000000
#define PM_RSTC_WRCFG_FULL_RESET 0x20

void rpi_reboot() {
    uart_flush_tx();

    PUT32(PM_WDOG, PM_PASSWORD | 1);
    PUT32(PM_RSTC, PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET);
}
void rpi_reset() {
    uart_putk("\r\nDONE!!!\n");
    rpi_reboot();
}

void assert(bool val, const char* msg) {
    if (!val) {
        uart_puts("\n[ERROR] Assertion failed: ");
        uart_puts(msg);
        rpi_reset();
    }
}
void panic(const char* msg) {
    uart_puts("\n[PANIC] ");
    uart_puts(msg);
    rpi_reset();
}

void printk(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt++);
            continue;
        }

        fmt++;

        switch (*fmt) {
            case 'c': {
                char c = (char) va_arg(args, int);
                uart_putc(c);
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char *);
                if (s) uart_puts(s);
                break;
            }
            case 'd': {
                int32_t x = va_arg(args, int32_t);
                uart_putd(x);
                break;
            }
            case 'x': {
                uint32_t x = va_arg(args, uint32_t);
                uart_putx(x);
                break;
            }
            case 'b': {
                uint32_t x = va_arg(args, uint32_t);
                uart_putb(x);
                break;
            }
            case 'f': {
                double d = va_arg(args, double);
                uart_putf((float) d);
                break;
            }
            case '%': {
                uart_putc('%');
                break;
            }
            default: {
                uart_putc('%');
                uart_putc(*fmt);
                break;
            }
        }

        fmt++;
    }

    va_end(args);
}

void mem_barrier_dsb() {
    asm volatile ("mcr p15, 0, r0, c7, c10, 4");
}
void mem_barrier_dmb() {
    asm volatile ("mcr p15, 0, r0, c7, c10, 5");
}

void* memcpy(void* dst, const void* src, uint32_t n) {
    // can't use 32-bit copies bc src and dst might not be the same alignment
    uint8_t* d = (uint8_t*) dst;
    const uint8_t* s = (const uint8_t*) src;
    while (n--) *d++ = *s++;

    return dst;
}
void* memset(void* dst, int val, uint32_t n) {
    val &= 0xFF;

    uint8_t* d = (uint8_t*) dst;
    while (((uintptr_t) d & 0x3) && n--) *d++ = (uint8_t) val;

    uint32_t* d32 = (uint32_t*) d;
    uint32_t val32 = (val << 24) | (val << 16) | (val << 8) | val;
    while (n >= 4) { *d32++ = val32; n -= 4; }

    d = (uint8_t*) d32;
    while (n--) *d++ = (uint8_t) val;

    return dst;
}
int memcmp(const void* s1, const void* s2, uint32_t n) {
    const uint8_t *a = s1, *b = s2;
    while (n-- > 0) {
        if (*a != *b) return *a - *b;
        a++, b++;
    }
    return 0;
}
