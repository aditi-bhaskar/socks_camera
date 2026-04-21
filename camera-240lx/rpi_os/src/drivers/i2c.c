#include "i2c.h"
#include "lib.h"
#include "gpio.h"
#include "mailbox_interface.h"

void i2c_enable(I2C* i2c) {
    OR32(i2c->bsc | I2C_C, I2C_ENABLE_BIT);
}
void i2c_disable(I2C* i2c) {
    AND32(i2c->bsc | I2C_C, ~I2C_ENABLE_BIT);
}
void i2c_init(I2C* i2c) {
    mem_barrier_dsb();

    // get clock rate from mailbox
    uint32_t core_clk_rate = mbox_get_clock_rate(MBOX_CLK_CORE);
    PUT32(i2c->bsc | I2C_DIV,
            (i2c->speed_hz ? (core_clk_rate / i2c->speed_hz) : 0));
    PUT32(i2c->bsc | I2C_A, i2c->slave_addr);
    i2c_clear_flags(i2c);

    Pin sda = i2c->sda;
    Pin scl = i2c->scl;

    mem_barrier_dsb();
    if (i2c->bsc == BSC0) {
        if ((sda.p_num == 0 && scl.p_num == 1) ||
                (sda.p_num == 28 && scl.p_num == 29)) {
            gpio_select(sda, GPIO_ALT_FUNC_0);
            gpio_select(scl, GPIO_ALT_FUNC_0);
        } else if (sda.p_num == 44 && scl.p_num == 45) {
            gpio_select(sda, GPIO_ALT_FUNC_1);
            gpio_select(scl, GPIO_ALT_FUNC_1);
        } else {
            panic("Invalid pins selected for BSC0");
        }
    } else if (i2c->bsc == BSC1) {
        if (sda.p_num == 2 && scl.p_num == 3) {
            gpio_select(sda, GPIO_ALT_FUNC_0);
            gpio_select(scl, GPIO_ALT_FUNC_0);
        } else if (sda.p_num == 44 && scl.p_num == 45) {
            gpio_select(sda, GPIO_ALT_FUNC_2);
            gpio_select(scl, GPIO_ALT_FUNC_2);
        } else {
            panic("Invalid pins selected for BSC1");
        }
    } else {
        panic("BSC2 disabled");
    }

    assert(!i2c_transfer_active(i2c), "Active I2C transfer after init");
    assert(i2c_rx_fifo_empty(i2c), "RX FIFO not empty after init");
    assert(i2c_tx_fifo_empty(i2c), "TX FIFO not empty after init");

    i2c_enable(i2c);

    mem_barrier_dsb();
}

void i2c_clear_flags(I2C* i2c) {
    PUT32(i2c->bsc | I2C_S, (I2C_DONE_BIT | I2C_ERR_BIT | I2C_CLKT_BIT));
}
void i2c_clear_fifo(I2C* i2c) {
    OR32(i2c->bsc | I2C_C, I2C_CLEAR_BIT);
}

bool i2c_tx_fifo_full(I2C* i2c) {
    return (GET32(i2c->bsc | I2C_S) & I2C_TXD_BIT) == 0;
}
bool i2c_tx_fifo_empty(I2C* i2c) {
    return (GET32(i2c->bsc | I2C_S) & I2C_TXE_BIT);
}
bool i2c_rx_fifo_empty(I2C* i2c) {
    return (GET32(i2c->bsc | I2C_S) & I2C_RXD_BIT) == 0;
}
bool i2c_transfer_active(I2C* i2c) {
    return (GET32(i2c->bsc | I2C_S) & I2C_TA_BIT);
}

bool i2c_transaction_done(I2C* i2c) {
    bool res = (GET32(i2c->bsc | I2C_S) & I2C_DONE_BIT);
    if (res) {
        PUT32(i2c->bsc | I2C_S, I2C_DONE_BIT);
    }
    return res;
}
I2C_Result i2c_check_error(I2C* i2c) {
    uint32_t status = GET32(i2c->bsc | I2C_S);

    if (status & I2C_ERR_BIT) {
        return I2C_RESULT_ERR;
    }
    if (status & I2C_CLKT_BIT) {
        return I2C_RESULT_CLKT;
    }

    return I2C_RESULT_OK;
}

I2C_Result i2c_send_data(I2C* i2c, uint32_t num_bytes, uint8_t* data) {
    assert((num_bytes & 0xFFFF) == num_bytes, "I2C num bytes larger than 16 bits");

    while (i2c_transfer_active(i2c));
    assert(!i2c_check_error(i2c), "Error found before I2C data write");

    i2c_clear_flags(i2c);
    PUT32(i2c->bsc | I2C_DLEN, num_bytes);
    AND32(i2c->bsc | I2C_C, ~I2C_READ_BIT);

    i2c_clear_fifo(i2c);
    uint32_t rec = 0;
    while (rec < num_bytes && !i2c_tx_fifo_full(i2c)) {
        PUT32(i2c->bsc | I2C_FIFO, data[rec++]);
    }

    OR32(i2c->bsc | I2C_C, I2C_ST_BIT);
    while (rec < num_bytes) {
        while (i2c_tx_fifo_full(i2c));
        PUT32(i2c->bsc | I2C_FIFO, data[rec++]);
    }

    I2C_Result res = I2C_RESULT_OK;
    while (!i2c_transaction_done(i2c) && !res) {
        res = i2c_check_error(i2c);
    }

    i2c_clear_flags(i2c);
    return res;
}
I2C_Result i2c_receive_data(I2C* i2c, uint32_t num_bytes, uint8_t* data) {
    assert((num_bytes & 0xFFFF) == num_bytes, "I2C num bytes larger than 16 bits");

    while (i2c_transfer_active(i2c));
    assert(!i2c_check_error(i2c), "Error found before I2C data read");

    i2c_clear_flags(i2c);
    PUT32(i2c->bsc | I2C_DLEN, num_bytes);
    OR32(i2c->bsc | I2C_C, I2C_READ_BIT);

    i2c_clear_fifo(i2c);
    OR32(i2c->bsc | I2C_C, I2C_ST_BIT);
    for (uint32_t rec = 0; rec < num_bytes; rec++) {
        while (i2c_rx_fifo_empty(i2c));
        data[rec] = GET32(i2c->bsc | I2C_FIFO);
    }

    I2C_Result res = I2C_RESULT_OK;
    while (!i2c_transaction_done(i2c) && !res) {
        res = i2c_check_error(i2c);
    }

    i2c_clear_flags(i2c);
    return res;
}
