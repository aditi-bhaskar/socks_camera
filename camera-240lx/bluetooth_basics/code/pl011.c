// Javier Garcia Nieto <jgnieto@cs.stanford.edu>. CS340LX Fall 2025.

#define CQ_N 131072

#include "rpi.h"
#include "pl011.h"
#include "circular.h"
#include "rpi-interrupts.h"
#include "gpio-high.h"

#include <stdbool.h>

#define GPIO_BT_CTS 30
#define GPIO_BT_RTS 31
#define GPIO_BT_TX 32
#define GPIO_BT_RX 33

#define ADDR(x) (0x20201000 + (x))

// Assuming 48 MHz UART clock (can be checked with mailbox), use 115200 baud
// 48,000,000 / (16 * (26 + 3/64)) = 115177, close enough to 115200
#define BAUD_INT_VAL 26
#define BAUD_FRAC_VAL 3 // setting to 0 would be closer to 43438 baud?

#define UART0_DR        ADDR(0x000)
#define UART0_RSRECR    ADDR(0x004)
#define UART0_FR        ADDR(0x018)
#define UART0_ILPR      ADDR(0x020)
#define UART0_IBRD      ADDR(0x024)
#define UART0_FBRD      ADDR(0x028)
#define UART0_LCRH      ADDR(0x02C)
#define UART0_CR        ADDR(0x030)
#define UART0_IFLS      ADDR(0x034)
#define UART0_IMSC      ADDR(0x038)
#define UART0_RIS       ADDR(0x03c)
#define UART0_MIS       ADDR(0x040)
#define UART0_ICR       ADDR(0x044)
#define UART0_DMACR     ADDR(0x048)
#define UART0_ITCR      ADDR(0x080)
#define UART0_ITIP      ADDR(0x084)
#define UART0_ITOP      ADDR(0x088)
#define UART0_TDR       ADDR(0x08c)

static struct {
    cq_t rx_buffer;
    bool initialized;
} module;


// Only called within interrupt so no need for device barriers
static uint8_t _get8(void) {
    // todo("Read a character from the data register. Make sure to mask out the error bits");
    return GET32(UART0_DR) & 0xff;
}

// Only called within interrupt so no need for device barriers
static int _has_data(void) {
    // todo("Use the FR register to check if data is available to read");
    // bit 4 is rxfe, 0 = data available
    return !(GET32(UART0_FR) & (1<<4));
}

void interrupt_vector(void) {
    dev_barrier();
    uint32_t mis = GET32(UART0_MIS);

    bool rx_interrupt = (mis & (1 << 4)) != 0;
    bool rx_timeout_interrupt = (mis & (1 << 6)) != 0;

    if (rx_interrupt || rx_timeout_interrupt) { // RX interrupt
        while (_has_data()) {
            cq_push(&module.rx_buffer, _get8());
        }
        if (rx_interrupt) {
            PUT32(UART0_ICR, (1<<4)); // 4th bit clears receive interrupt on rxic
            // todo("Clear RX interrupt in ICR register");
        }
        if (rx_timeout_interrupt) {
            PUT32(UART0_ICR, (1<<6)); // 4th bit clears timeout interrupt on rtic
            // todo("Clear RX timeout interrupt in ICR register");
        }
    } else {
        panic("Unexpected UART interrupt: MIS=0x%X\n", mis);
    }
    dev_barrier();
}

void pl011_init(void) {
    assert(!module.initialized);
    module.initialized = true;

    cq_init(&module.rx_buffer, 1);

    while (pl011_has_data())
        pl011_get8(); // flush any spurious data

    pl011_flush_tx();

    // disable UART
    PUT32(UART0_CR, 0);

    dev_barrier();

    
    // todo("Set up GPIO pins for UART0 functionality and disable pull-ups/downs."
    //         "We need all 4 pins. Remember to use gpio_hi.");
    // gpio pins 30 to 33 are UART0 on GPIO_FUNC_ALT3 mode
    gpio_hi_set_function(GPIO_BT_CTS, GPIO_FUNC_ALT3); // pin 30
    gpio_hi_set_function(GPIO_BT_RTS, GPIO_FUNC_ALT3); // pin 31
    gpio_hi_set_function(GPIO_BT_TX,  GPIO_FUNC_ALT3); // pin 32
    gpio_hi_set_function(GPIO_BT_RX,  GPIO_FUNC_ALT3); // pin 33
    gpio_hi_pud_off(GPIO_BT_CTS);
    gpio_hi_pud_off(GPIO_BT_RTS);
    gpio_hi_pud_off(GPIO_BT_TX);
    gpio_hi_pud_off(GPIO_BT_RX);

    dev_barrier();

    interrupt_init();

    // Set up UART
    // todo("Clear the interrupts using the ICR register");
    PUT32(UART0_ICR, 0x7FF);                    // clear pending interrupts
    // todo("Set the integer & fractional baud rate registers (IBRD, FBRD)");
    PUT32(UART0_IBRD, BAUD_INT_VAL);            // int baud divisor is 6
    PUT32(UART0_FBRD, BAUD_FRAC_VAL);           // frac baud divisor is 3
    
    // todo("Set the interrupt mask register (IMSC) to enable RX and RX timeout interrupts");
    PUT32(UART0_IMSC, (1 << 4) | (1 << 6));    // enable RXIM at 1<<4 and RTIM 1<<6
    // todo("Set the interrupt FIFO level select register (IFLS) to generate interrupts as soon as data is available");
    PUT32(UART0_IFLS, 0);                       // RXIFLSEL is 000: 
    // interrupt when we are over 1/8 full (set it as a minimum)

    // todo("Set the line control register (LCRH) to 8n1 and enable FIFOs");
    // set WLEN=11 (8 bits, bits[6:5]), 
    // set FEN=1 (FIFO enable, bit4), no parity, 1 stop bit
    PUT32(UART0_LCRH, (3 << 5) | (1 << 4));

    // Enable UART
    PUT32(UART0_CR, 0xb01);  // enable RTS, TX, RX, UART

    dev_barrier();

    // todo("Write to the IRQ_Enable_2 register to enable UART interrupts");
    // IRQ 57 = 32+25
    // this means bit 25 of IRQ_Enable_2 (0x2000B214) enables UART0 interrupt
    PUT32(0x2000B214, (1 << 25));
    enable_interrupts();

    dev_barrier();
}

void pl011_set_baud(unsigned baud) {
    assert(module.initialized);

    // BAUDDIV = 64 * FUARTCLK / (16 * baud) = 4 * FUARTCLK / baud (in 1/64ths).
    // Add baud/2 first so the integer divide rounds to nearest.
    // For 115200 this gives IBRD=26, FBRD=3 (matches the init defaults).
    unsigned bauddiv = (4u * 48000000u + baud / 2) / baud;
    unsigned ibrd = bauddiv >> 6;
    unsigned fbrd = bauddiv & 0x3f;

    pl011_flush_tx();           // drain anything still queued at the old baud

    dev_barrier();
    PUT32(UART0_CR, 0);         // disable UART while we reprogram it
    dev_barrier();

    PUT32(UART0_IBRD, ibrd);
    PUT32(UART0_FBRD, fbrd);
    // LCRH MUST be written last. IBRD/FBRD are only latched into the live baud
    // generator on the LCRH write strobe (ARM DDI0183G 3.3.7). Writing LCRH
    // before the divisors is the classic intermittent-garbage bug.
    PUT32(UART0_LCRH, (3 << 5) | (1 << 4));  // 8n1, FIFO enable

    PUT32(UART0_CR, 0xb01);     // re-enable RTS, TX, RX, UART
    dev_barrier();
}

int pl011_can_put8(void) {
    assert(module.initialized);
    dev_barrier();
    int ret = !(GET32(UART0_FR) & (1 << 5)); // bit 5 is txff, 0 = space available.
    // todo("Check that device can accept a character for transmission");
    dev_barrier();
    return ret;
}

void pl011_put8(uint8_t c) {
    while (!pl011_can_put8())
        rpi_wait();
    // todo("Write the character to the data register");
    PUT32(UART0_DR, c);
    dev_barrier();
}


int pl011_tx_is_empty(void) {
    assert(module.initialized);
    dev_barrier();

    uint32_t uart_fr = GET32(UART0_FR);
    // bit7 is txfe (tx fifo empty)
    // bit3 is busy (0 =- idle)
    int ret = (uart_fr & (1 << 7)) && !(uart_fr & (1 << 3));
    // todo("Check that device is not busy and empty");
    dev_barrier();
    return ret;
}

// Below are functions we provide
int pl011_has_data(void) {
    assert(module.initialized);
    return !cq_empty(&module.rx_buffer);
}

uint8_t pl011_get8(void) {
    assert(module.initialized);
    return cq_pop(&module.rx_buffer);
}


void pl011_flush_tx(void) {
    assert(module.initialized);
    while (!pl011_tx_is_empty())
        rpi_wait();
}

int pl011_get8_async(void) { 
    assert(module.initialized);
    if (!pl011_has_data())
        return -1;
    return pl011_get8();
}

