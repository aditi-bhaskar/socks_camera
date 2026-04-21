#ifndef I2C_H
#define I2C_H

#include "gpio.h"

typedef enum BSC_t {
    BSC0 = 0x20205000 | KERNEL_VBASE,
    BSC1 = 0x20804000 | KERNEL_VBASE,
    BSC2 = 0x20805000 | KERNEL_VBASE, // used in HDMI interface, avoid

    IDLE    = 0,
    READING = 1,
} BSC;

// SDA0/SCL0: pin 0/1 AF0 or pin 28/29 AF0 or 44/45 AF1
// SDA1/SCL1: pin 2/3 AF0 or 44/45 AF2

// address offsets
enum {
    I2C_C       = 0x00,
    I2C_S       = 0x04,
    I2C_DLEN    = 0x08,
    I2C_A       = 0x0C,
    I2C_FIFO    = 0x10,
    I2C_DIV     = 0x14,
    I2C_DEL     = 0x18,
    I2C_CLKT    = 0x1C
};

enum {
    I2C_READ_BIT    = 1 << 0,
    I2C_CLEAR_BIT   = 1 << 4,
    I2C_ST_BIT      = 1 << 7,
    I2C_INTD_BIT    = 1 << 8,
    I2C_INTT_BIT    = 1 << 9,
    I2C_INTR_BIT    = 1 << 10,
    I2C_ENABLE_BIT  = 1 << 15,

    I2C_TA_BIT      = 1 << 0,
    I2C_DONE_BIT    = 1 << 1,
    I2C_TXW_BIT     = 1 << 2,
    I2C_RXR_BIT     = 1 << 3,
    I2C_TXD_BIT     = 1 << 4,
    I2C_RXD_BIT     = 1 << 5,
    I2C_TXE_BIT     = 1 << 6,
    I2C_RXF_BIT     = 1 << 7,
    I2C_ERR_BIT     = 1 << 8,
    I2C_CLKT_BIT    = 1 << 9,
};

typedef enum {
    I2C_RESULT_OK,
    I2C_RESULT_ERR,
    I2C_RESULT_CLKT
} I2C_Result;

typedef struct { // should define a separate I2C for each slave
    BSC bsc; // double as reading/idle status for software I2C
    Pin sda;
    Pin scl;
    uint32_t speed_hz; // default 0 will set 100kHz
    uint32_t slave_addr;
} I2C;

void i2c_enable(I2C* i2c);
void i2c_disable(I2C* i2c);
void i2c_init(I2C* i2c);

void i2c_clear_flags(I2C* i2c);
void i2c_clear_fifo(I2C* i2c);
bool i2c_tx_fifo_full(I2C* i2c);
bool i2c_tx_fifo_empty(I2C* i2c);
bool i2c_rx_fifo_empty(I2C* i2c);
bool i2c_transfer_active(I2C* i2c);
bool i2c_transaction_done(I2C* i2c);
I2C_Result i2c_check_error(I2C* i2c);

I2C_Result i2c_send_data(I2C* i2c, uint32_t num_bytes, uint8_t* data);
I2C_Result i2c_receive_data(I2C* i2c, uint32_t num_bytes, uint8_t* data);

void i2c_sw_init(I2C* i2c);
void i2c_sw_start_cond(I2C* i2c);
void i2c_sw_stop_cond(I2C* i2c);
void i2c_sw_write_bit(I2C* i2c, bool bit);
bool i2c_sw_write_byte(I2C* i2c, uint8_t data);
bool i2c_sw_read_bit(I2C* i2c);
uint8_t i2c_sw_read_byte(I2C* i2c, bool done_p);

void i2c_sw_read(I2C* i2c, uint32_t num_bytes, uint8_t* data);
void i2c_sw_write(I2C* i2c, uint32_t num_bytes, uint8_t* data);

bool i2c_is_sw(I2C* i2c);

#endif
