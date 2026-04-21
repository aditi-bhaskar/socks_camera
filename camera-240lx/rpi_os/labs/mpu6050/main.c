#include "mpu6050.h"
#include "sys_timer.h"
#include "lib.h"

extern uint32_t irq_ptr;

const Pin sda = { 2 };
const Pin scl = { 3 };
I2C i2c = {
    .bsc = BSC1,
    .scl = scl,
    .sda = sda,
    .slave_addr = MPU6050_ADDR,
    .speed_hz = 400000
};

const Pin mpu_int = { 4 };
MPU6050 mpu = {
    .i2c = &i2c,
    .int_pin = mpu_int,

    .accel_sel = MPU_ACCEL_FS_2G,
    .gyro_sel = MPU_GYRO_FS_1000DPS,
    .dlpf = MPU_DLPF_260HZ
};

__attribute__((interrupt("IRQ")))
void irq_handler() {
    if (gpio_has_interrupt()) {
        mpu6050_isr(&mpu);
    }
}

void main() {
    irq_ptr = (uint32_t) irq_handler;

    mpu6050_config(&mpu);

    printk("%x\n", mpu6050_get_addr(&mpu));

    /*
    while (1) {
        float x = mpu6050_read_accel_x(&mpu);
        float y = mpu6050_read_accel_y(&mpu);
        float z = mpu6050_read_accel_z(&mpu);
        printk("x: %f\ny: %f\nz: %f\n", x, y, z);

        sys_timer_delay_ms(100);
    }
    */

    mpu6050_self_test(&mpu);
}

