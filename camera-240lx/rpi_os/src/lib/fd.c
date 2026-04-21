#include "fd.h"
#include "uart.h"
#include "lib.h"

#include <stddef.h>

static int console_read(File* f, void* buf, uint32_t n) {
    char* s = (char*) buf;
    uint32_t read = 0;

    for (uint32_t i = 0; i < n; i++) {
        char c = uart_getc();
        if (c == '\r') {
            c = '\n';
        }

        if (c == '\n') {
            uart_putc('\r');
        }
        uart_putc(c);

        s[i] = c;
        read++;

        if (c == '\n') {
            break;
        }
    }
    
    return read;
}
static int console_write(File* f, const void* buf, uint32_t n) {
    const char* s = (const char*) buf;
    for (uint32_t i = 0; i < n; i++) {
        if (s[i] == '\n') {
            uart_putc('\r');
        }
        uart_putc(s[i]);
    }
    return n;
}
static int console_ioctl(File* f, int req, void* arg) {
    return -ENOTTY;
}

FileOps console_ops = {
    .read = console_read,
    .write = console_write,
    .ioctl = console_ioctl,
    .close = NULL
};

VNode console_vnode = {
    .ops = &console_ops,
    .data = NULL
};

