#include "i2c.h"
#include "sys_timer.h"
#include "lib.h"

static PinOutput read_scl(I2C* i2c) {
    return gpio_read(i2c->scl);
}
static PinOutput read_sda(I2C* i2c) {
    return gpio_read(i2c->sda);
}
static void set_scl(I2C* i2c) {
    gpio_select_input(i2c->scl);
}
static void set_sda(I2C* i2c) {
    gpio_select_input(i2c->sda);
}
static void clear_scl(I2C* i2c) {
    gpio_select_output(i2c->scl);
    gpio_set_low(i2c->scl);
}
static void clear_sda(I2C* i2c) {
    gpio_select_output(i2c->sda);
    gpio_set_low(i2c->sda);
}
static void delay(I2C* i2c) {
    // TODO: calculate delay?
    sys_timer_delay_us(4);
}
static void clock_stretching(I2C* i2c) {
    uint32_t t = sys_timer_get_usec();
    while (!read_scl(i2c) && (sys_timer_get_usec() - t < 1000));
}

void i2c_sw_init(I2C* i2c) {
    gpio_set_pull(i2c->sda, GPIO_PULL_UP);
    gpio_select_input(i2c->sda);
    gpio_set_pull(i2c->scl, GPIO_PULL_UP);
    gpio_select_input(i2c->scl);

    i2c->bsc = IDLE;
    assert((i2c->slave_addr & 0x7F) == i2c->slave_addr, "I2C invalid slave address");
}
void i2c_sw_start_cond(I2C* i2c) {
    if (i2c->bsc == READING) {
        set_sda(i2c);
        delay(i2c);
        set_scl(i2c);
        clock_stretching(i2c);
        delay(i2c);
    }

    assert(read_sda(i2c), "Software I2C startup: arbitration lost");

    clear_sda(i2c);
    delay(i2c);
    clear_scl(i2c);
    i2c->bsc = READING;
}
void i2c_sw_stop_cond(I2C* i2c) {
    clear_sda(i2c);
    delay(i2c);

    set_scl(i2c);
    clock_stretching(i2c);
    delay(i2c);

    set_sda(i2c);
    delay(i2c);

    assert(read_sda(i2c), "Software I2C stop: arbitration lost");
    i2c->bsc = IDLE;
}
void i2c_sw_write_bit(I2C* i2c, bool bit) {
    if (bit) {
        set_sda(i2c);
    } else {
        clear_sda(i2c);
    }

    delay(i2c);
    set_scl(i2c);
    delay(i2c);
    clock_stretching(i2c);

    if (bit && !read_sda(i2c)) {
        panic("Software I2C write: arbitration lost");
    }

    clear_scl(i2c);
}
bool i2c_sw_write_byte(I2C* i2c, uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        i2c_sw_write_bit(i2c, (data >> i) & 1);
    }

    return i2c_sw_read_bit(i2c);
}
bool i2c_sw_read_bit(I2C* i2c) {
    set_sda(i2c);
    delay(i2c);
    set_scl(i2c);
    clock_stretching(i2c);
    delay(i2c);

    bool res = read_sda(i2c);
    clear_scl(i2c);

    return res;
}
uint8_t i2c_sw_read_byte(I2C* i2c, bool done_p) {
    uint8_t res = 0;
    for (int i = 0; i < 8; i++) {
        res = (res << 1) | i2c_sw_read_bit(i2c);
    }

    i2c_sw_write_bit(i2c, done_p);
    return res;
}
void i2c_sw_read(I2C* i2c, uint32_t num_bytes, uint8_t* data) {
    i2c_sw_start_cond(i2c);

    if (i2c_sw_write_byte(i2c, (i2c->slave_addr << 1) | 1)) {
        panic("Failed writing address during read");
    }

    for (uint32_t i = 0; i < num_bytes; i++) {
        data[i] = i2c_sw_read_byte(i2c, i == num_bytes - 1);
    }

    i2c_sw_stop_cond(i2c);
}
void i2c_sw_write(I2C* i2c, uint32_t num_bytes, uint8_t* data) {
    i2c_sw_start_cond(i2c);

    if (i2c_sw_write_byte(i2c, (i2c->slave_addr << 1))) {
        panic("Failed writing address during write");
    }

    for (uint32_t i = 0; i < num_bytes; i++) {
        if (i2c_sw_write_byte(i2c, data[i])) {
            panic("Failed writing data");
        }
    }

    i2c_sw_stop_cond(i2c);
}

bool i2c_is_sw(I2C* i2c) {
    return i2c->bsc == IDLE || i2c->bsc == READING;
}
