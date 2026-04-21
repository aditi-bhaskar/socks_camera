#include "lib.h"
#include "gpio.h"
#include "math.h"
#include "uart.h"
#include "sys_timer.h"
#include "arm_timer.h"

#define BUF_LEN 2048
#define N_DIVS 100
#define NXT(x) ((x + 1) % BUF_LEN)

#define STREAM

extern uint32_t irq_ptr;

typedef enum {
    ZERO,
    ONE,
    HEADER,
    STOP,
    NONE
} IR_val;
const uint32_t vals[3][2] = {
    { 600, 600 },
    { 600, 1600 },
    { 9000, 4500 },
};
const int32_t diffs[3] = {
    150,
    150,
    1500,
};

typedef enum {
    RM_CH_D  =	0xBA45FF00,
    RM_CH    =	0xB946FF00,
    RM_CH_U  =	0xB847FF00,
    RM_PREV  =	0xBB44FF00,
    RM_NEXT  =	0xBF40FF00,
    RM_PLAY  =	0xBC43FF00,
    RM_VOL_D =	0xF807FF00,
    RM_VOL_U =	0xEA15FF00,
    RM_EQ    =	0xF609FF00,
    RM_0     =	0xE916FF00,
    RM_100_P =	0xE619FF00,
    RM_200_P =	0xF20DFF00,
    RM_1     =	0xF30CFF00,
    RM_2     =	0xE718FF00,
    RM_3     =	0xA15EFF00,
    RM_4     =	0xF708FF00,
    RM_5     =	0xE31CFF00,
    RM_6     =	0xA55AFF00,
    RM_7     =	0xBD42FF00,
    RM_8     =	0xAD52FF00,
    RM_9     =	0xB54AFF00,
} RM_val;

uint32_t set_percentage(uint32_t decoding) {
    switch (decoding) {
        case RM_1:     return 10;
        case RM_2:     return 20;
        case RM_3:     return 30;
        case RM_4:     return 40;
        case RM_5:     return 50;
        case RM_6:     return 60;
        case RM_7:     return 70;
        case RM_8:     return 80;
        case RM_9:     return 90;
        case RM_0:     return 100;
        default: return 0;
    }
}

const RM_val RM_vals[] = {
    RM_CH_D, RM_CH, RM_CH_U,
    RM_PREV, RM_NEXT, RM_PLAY, RM_VOL_D, RM_VOL_U,
    RM_EQ, RM_0, RM_100_P, RM_200_P,
    RM_1, RM_2, RM_3, RM_4, RM_5,
    RM_6, RM_7, RM_8, RM_9
};
const char* RM_val_names[] = {
    "CH-", "CH", "CH+",
    "PREV", "NEXT", "PLAY", "VOL-", "VOL+",
    "EQ", "0", "100+", "200+",
    "1", "2", "3", "4", "5",
    "6", "7", "8", "9", "NONE"
};


Pin p = { 21 }, led = { 20 };
volatile uint32_t last_t, cur_idx, lst_header, done;
uint32_t val[BUF_LEN], dur[BUF_LEN];
IR_val ops[BUF_LEN];

IR_val get_ir_val(uint32_t idx) {
    if (val[idx]) {
        return NONE;
    }

    uint32_t v0 = dur[idx], v1 = dur[NXT(idx)];
    if (val[NXT(idx)]) {
        for (int i = 0; i < 3; i++) {
            int32_t d0 = (v0 - vals[i][0]),
                    d1 = (v1 - vals[i][1]);

            if (d0 >= -diffs[i] && d0 <= diffs[i] && d1 >= -diffs[i] && d1 <= diffs[i]) {
                return (IR_val) i;
            }
        }
    }

    if (v0 >= 450 && v0 <= 750) {
        return STOP;
    }
    return NONE;
}
const char* get_rm_name(uint32_t x) {
    for (int i = 0; i < 21; i++) {
        if (x == RM_vals[i]) {
            return RM_val_names[i];
        }
    }
    return RM_val_names[21];
}

uint32_t counter, pct;
__attribute__((interrupt("IRQ")))
void irq_handler() {
    if (gpio_has_interrupt()) {
        if (gpio_event_detected(p)) {
            uint32_t t = sys_timer_get_usec();

            val[cur_idx] = !gpio_read(p);
            dur[cur_idx] = t - last_t;
            last_t = t;
            cur_idx = NXT(cur_idx);
            gpio_event_clear(p);
        }
    } else if (uart_has_interrupt()) {
        if ((GET32(AUX_IRQ) & 1) &&
             (((GET32(AUX_MU_IIR_REG) >> 1) & 3) == 0b10)) {
            if ((GET32(AUX_MU_IO_REG) & 0xFF) == 'q') {
                done = 1;
            }
        }
    } else if (arm_timer_has_interrupt()) {
        counter = (counter + 1) % 100;

        if (counter < pct) {
            gpio_set_high(led);
        } else {
            gpio_set_low(led);
        }

        arm_timer_int_clear();
    }
}

void main() {
    irq_ptr = (uint32_t) irq_handler;

    gpio_enable_int_rising_edge(p);
    gpio_enable_int_falling_edge(p);
    gpio_select_input(p);

    gpio_select_output(led);

    uart_enable_rx_interrupts();

    arm_timer_set_freq(200 * N_DIVS);
    arm_timer_enable();

    mem_barrier_dsb();

    uint32_t idx = 0, op_idx = 0, presses = 0;
    while (!done && op_idx < BUF_LEN) {
        if (idx + 2 <= cur_idx || cur_idx < idx) {
            IR_val res = get_ir_val(idx);
            idx = NXT(idx);

            if (res != NONE) {
                if (res == HEADER) {
                    lst_header = op_idx;
                    printk("%d\n", ++presses);
                }

                ops[op_idx++] = res;
                idx = NXT(idx);

#ifdef STREAM
                if (lst_header == op_idx - 33) {
                    uint32_t x = 0;
                    for (int j = 0; j < 32; j++) {
                        x |= (ops[op_idx - 32 + j] << j);
                    }

                    printk("%x\t%s\n", x, get_rm_name(x));

                    pct = set_percentage(x);
                }
#endif
            }
        }
    }

#ifndef STREAM
    while (idx != cur_idx) {
        IR_val res = get_ir_val(idx);
        idx = NXT(idx);

        if (res != NONE) {
            ops[op_idx++] = res;

            if (idx != cur_idx) {
                idx = NXT(idx);
            }
        }
    }

    int c = 0;
    for (uint32_t i = 0; i + 33 <= op_idx; i++) {
        if (ops[i] == HEADER && ops[i + 33] == STOP) {
            uint32_t x = 0;
            for (int j = 0; j < 32; j++) {
                x |= (ops[i + j + 1] << j);
            }

            printk("press %d:\t%x\t%s\n", ++c, x, get_rm_name(x));
            i += 32;
        }
    }

    printk("missed presses: %d\n", presses - c);
#endif
}
