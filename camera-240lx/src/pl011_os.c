// pl011_os.c — PL011 UART driver for the BT chip, polling port of pl011.c for
// rpi_os. two differences from the libpi original: 
// (1) polling, not interrupts (reads the FIFO directly; RTS/CTS flow control keeps the 16-byte RX FIFO from overflowing);
// (2) registers are accessed at (phys | KERNEL_VBASE) since the camera build runs with the MMU on (see port/rpi.h PORT_PERI).

#include "rpi.h"          // port shim
#include "pl011.h"
#include "gpio-high.h"    // port VBASE version

#include <stdbool.h>

#define GPIO_BT_CTS 30
#define GPIO_BT_RTS 31
#define GPIO_BT_TX  32
#define GPIO_BT_RX  33

// UART0 (PL011) base, mapped through the kernel virtual base.
#define ADDR(x) (PORT_PERI(0x20201000) + (x))

#define BAUD_INT_VAL  26
#define BAUD_FRAC_VAL 3

#define UART0_DR        ADDR(0x000)
#define UART0_FR        ADDR(0x018)
#define UART0_IBRD      ADDR(0x024)
#define UART0_FBRD      ADDR(0x028)
#define UART0_LCRH      ADDR(0x02C)
#define UART0_CR        ADDR(0x030)
#define UART0_IFLS      ADDR(0x034)
#define UART0_IMSC      ADDR(0x038)
#define UART0_ICR       ADDR(0x044)

static struct {
    bool initialized;
} module;


// note to self: remember dev barriers everywhere
// ---- low-level FIFO access -------------------------------------------------

int pl011_has_data(void) {
    assert(module.initialized);
    dev_barrier();

    // FR bit 4 = RXFE (rx fifo empty); 0 means data available.
    int ret = !(GET32(UART0_FR) & (1 << 4));
    dev_barrier();
    return ret;
}

uint8_t pl011_get8(void) {
    assert(module.initialized);
    while (!pl011_has_data()) {
        rpi_wait();
    }
    dev_barrier();
    uint8_t c = GET32(UART0_DR) & 0xff;
    dev_barrier();
    return c;
}

int pl011_get8_async(void) {
    assert(module.initialized);
    if (!pl011_has_data())
        return -1;
    return pl011_get8();
}

int pl011_can_put8(void) {
    assert(module.initialized);
    dev_barrier();

    // FR bit 5 = TXFF (tx fifo full); 0 means space available.
    int ret = !(GET32(UART0_FR) & (1 << 5));
    dev_barrier();
    return ret;
}

void pl011_put8(uint8_t c) {
    while (!pl011_can_put8())
        rpi_wait();
    PUT32(UART0_DR, c);
    dev_barrier();
}

int pl011_tx_is_empty(void) {
    assert(module.initialized);
    dev_barrier();
    uint32_t fr = GET32(UART0_FR);
    // bit 7 = TXFE (tx fifo empty), bit 3 = BUSY (0 = idle).
    int ret = (fr & (1 << 7)) && !(fr & (1 << 3));
    dev_barrier();
    return ret;
}

void pl011_flush_tx(void) {
    assert(module.initialized);
    while (!pl011_tx_is_empty())
        rpi_wait();
}

// ---- init / baud -----------------------------------------------------------

void pl011_init(void) {
    assert(!module.initialized);
    module.initialized = true;

    // disable UART while we configure it.
    dev_barrier();
    PUT32(UART0_CR, 0);
    dev_barrier();

    // pins 30-33 = UART0 (ALT3): CTS, RTS, TX, RX. Disable pulls.
    gpio_hi_set_function(GPIO_BT_CTS, GPIO_FUNC_ALT3);
    gpio_hi_set_function(GPIO_BT_RTS, GPIO_FUNC_ALT3);
    gpio_hi_set_function(GPIO_BT_TX,  GPIO_FUNC_ALT3);
    gpio_hi_set_function(GPIO_BT_RX,  GPIO_FUNC_ALT3);
    gpio_hi_pud_off(GPIO_BT_CTS);
    gpio_hi_pud_off(GPIO_BT_RTS);
    gpio_hi_pud_off(GPIO_BT_TX);
    gpio_hi_pud_off(GPIO_BT_RX);

    dev_barrier();

    PUT32(UART0_ICR, 0x7FF);                  // clear pending interrupts
    PUT32(UART0_IBRD, BAUD_INT_VAL);          // 115200 baud divisors
    PUT32(UART0_FBRD, BAUD_FRAC_VAL);
    PUT32(UART0_IMSC, 0);                      // polling: mask all interrupts
    PUT32(UART0_IFLS, 0);
    PUT32(UART0_LCRH, (3 << 5) | (1 << 4));    // 8n1, FIFO enable
    PUT32(UART0_CR, 0xb01);                     // enable RTS, TX, RX, UART

    dev_barrier();

    // flush any spurious bytes that arrived during bring-up.
    while (pl011_has_data())
        (void)pl011_get8();
}

void pl011_set_baud(unsigned baud) {
    assert(module.initialized);

    // BAUDDIV (in 1/64ths) = 4 * FUARTCLK / baud, rounded to nearest.
    unsigned bauddiv = (4u * 48000000u + baud / 2) / baud;
    unsigned ibrd = bauddiv >> 6;
    unsigned fbrd = bauddiv & 0x3f;

    pl011_flush_tx();

    dev_barrier();
    PUT32(UART0_CR, 0);
    dev_barrier();

    PUT32(UART0_IBRD, ibrd);
    PUT32(UART0_FBRD, fbrd);

    // LCRH must be written last to latch IBRD/FBRD (ARM DDI0183G 3.3.7).
    PUT32(UART0_LCRH, (3 << 5) | (1 << 4));
    PUT32(UART0_CR, 0xb01);
    dev_barrier();
}
