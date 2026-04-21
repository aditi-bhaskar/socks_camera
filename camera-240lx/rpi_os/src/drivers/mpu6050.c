#include "mpu6050.h"
#include "gpio.h"
#include "sys_timer.h"
#include "lib.h"
#include "math.h"

void mpu6050_config(MPU6050* mpu) {
    mem_barrier_dsb();

    if (i2c_is_sw(mpu->i2c)) {
        printk("Using bit-banged I2C\n");
        i2c_sw_init(mpu->i2c);
    } else {
        printk("Using hardware I2C\n");
        i2c_init(mpu->i2c);
        i2c_enable(mpu->i2c);
    }

    mem_barrier_dsb();

    mpu6050_reset(mpu);

    mem_barrier_dsb();

    mpu6050_write_reg(mpu, MPU_USER_CTRL,
            MPU_USER_CTRL_SIG_COND_RESET |
            MPU_USER_CTRL_FIFO_RESET |
            MPU_USER_CTRL_FIFO_EN);

    sys_timer_delay_ms(100);

    mpu6050_write_reg(mpu, MPU_PWR_MGMT_1, MPU_PWR1_CLKSEL_INTERNAL);

    mpu6050_write_reg(mpu, MPU_CONFIG, mpu->dlpf);
    mpu6050_write_reg(mpu, MPU_GYRO_CONFIG, mpu->gyro_sel);
    mpu6050_write_reg(mpu, MPU_ACCEL_CONFIG, mpu->accel_sel);

    mpu6050_write_reg(mpu, MPU_INT_ENABLE, MPU_INT_DATA_RDY_EN);

    mem_barrier_dsb();

    gpio_select(mpu->int_pin, GPIO_INPUT);
    gpio_enable_int_rising_edge(mpu->int_pin);

    mem_barrier_dsb();
}
void mpu6050_reset(MPU6050* mpu) {
    mpu6050_write_reg(mpu, MPU_PWR_MGMT_1, MPU_PWR1_DEVICE_RESET);
    while (mpu6050_read_reg(mpu, MPU_PWR_MGMT_1));
    sys_timer_delay_ms(100);
}

int16_t mpu6050_read_gyro_x_raw(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_GYRO_X);
    while (!mpu->data_ready);

    uint16_t res = (mpu6050_read_reg(mpu, MPU_GYRO_XOUT_H) << 8)
        | mpu6050_read_reg(mpu, MPU_GYRO_XOUT_L);

    mpu->data_ready = false;
    return res;

}
int16_t mpu6050_read_gyro_y_raw(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_GYRO_Y);
    while (!mpu->data_ready);

    uint16_t res = (mpu6050_read_reg(mpu, MPU_GYRO_YOUT_H) << 8)
        | mpu6050_read_reg(mpu, MPU_GYRO_YOUT_L);

    mpu->data_ready = false;
    return res;
}
int16_t mpu6050_read_gyro_z_raw(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_GYRO_Z);
    while (!mpu->data_ready);

    uint16_t res = (mpu6050_read_reg(mpu, MPU_GYRO_ZOUT_H) << 8)
        | mpu6050_read_reg(mpu, MPU_GYRO_ZOUT_L);

    mpu->data_ready = false;
    return res;
}
float mpu6050_read_gyro_x(MPU6050* mpu) {
    int16_t raw = mpu6050_read_gyro_x_raw(mpu);
    switch (mpu->gyro_sel) {
        case MPU_GYRO_FS_250DPS:
            return raw / 131.f;
        case MPU_GYRO_FS_500DPS:
            return raw / 65.5f;
        case MPU_GYRO_FS_1000DPS:
            return raw / 32.8f;
        case MPU_GYRO_FS_2000DPS:
            return raw / 16.4f;
    }
    return 0;
}
float mpu6050_read_gyro_y(MPU6050* mpu) {
    int16_t raw = mpu6050_read_gyro_y_raw(mpu);
    switch (mpu->gyro_sel) {
        case MPU_GYRO_FS_250DPS:
            return raw / 131.f;
        case MPU_GYRO_FS_500DPS:
            return raw / 65.5f;
        case MPU_GYRO_FS_1000DPS:
            return raw / 32.8f;
        case MPU_GYRO_FS_2000DPS:
            return raw / 16.4f;
    }
    return 0;
}
float mpu6050_read_gyro_z(MPU6050* mpu) {
    int16_t raw = mpu6050_read_gyro_z_raw(mpu);
    switch (mpu->gyro_sel) {
        case MPU_GYRO_FS_250DPS:
            return raw / 131.f;
        case MPU_GYRO_FS_500DPS:
            return raw / 65.5f;
        case MPU_GYRO_FS_1000DPS:
            return raw / 32.8f;
        case MPU_GYRO_FS_2000DPS:
            return raw / 16.4f;
    }
    return 0;
}

int16_t mpu6050_read_accel_x_raw(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_ACCEL);
    while (!mpu->data_ready);

    uint8_t data[6];
    for (int i = 0; i < 6; i++) {
        data[i] = mpu6050_read_reg(mpu, MPU_ACCEL_XOUT_H + i);
    }
    mpu->data_ready = false;

    return (data[0] << 8) | data[1];
}
int16_t mpu6050_read_accel_y_raw(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_ACCEL);
    while (!mpu->data_ready);

    uint8_t data[6];
    for (int i = 0; i < 6; i++) {
        data[i] = mpu6050_read_reg(mpu, MPU_ACCEL_XOUT_H + i);
    }
    mpu->data_ready = false;

    return (data[2] << 8) | data[3];
}
int16_t mpu6050_read_accel_z_raw(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_ACCEL);
    while (!mpu->data_ready);

    uint8_t data[6];
    for (int i = 0; i < 6; i++) {
        data[i] = mpu6050_read_reg(mpu, MPU_ACCEL_XOUT_H + i);
    }
    mpu->data_ready = false;

    return (data[4] << 8) | data[5];
}

float mpu6050_read_accel_x(MPU6050* mpu) {
    int16_t raw = mpu6050_read_accel_x_raw(mpu);
    switch (mpu->accel_sel) {
        case MPU_ACCEL_FS_2G:
            return raw / 16384.f;
        case MPU_ACCEL_FS_4G:
            return raw / 8192.f;
        case MPU_ACCEL_FS_8G:
            return raw / 4096.f;
        case MPU_ACCEL_FS_16G:
            return raw / 2048.f;
    }
    return 0;
}
float mpu6050_read_accel_y(MPU6050* mpu) {
    int16_t raw = mpu6050_read_accel_y_raw(mpu);
    switch (mpu->accel_sel) {
        case MPU_ACCEL_FS_2G:
            return raw / 16384.f;
        case MPU_ACCEL_FS_4G:
            return raw / 8192.f;
        case MPU_ACCEL_FS_8G:
            return raw / 4096.f;
        case MPU_ACCEL_FS_16G:
            return raw / 2048.f;
    }
    return 0;
}
float mpu6050_read_accel_z(MPU6050* mpu) {
    int16_t raw = mpu6050_read_accel_z_raw(mpu);
    switch (mpu->accel_sel) {
        case MPU_ACCEL_FS_2G:
            return raw / 16384.f;
        case MPU_ACCEL_FS_4G:
            return raw / 8192.f;
        case MPU_ACCEL_FS_8G:
            return raw / 4096.f;
        case MPU_ACCEL_FS_16G:
            return raw / 2048.f;
    }
    return 0;
}

int16_t mpu6050_read_temp(MPU6050* mpu) {
    mpu->data_ready = false;
    mpu6050_write_reg(mpu, MPU_FIFO_EN, MPU_FIFO_EN_TEMP);
    while (!mpu->data_ready);

    uint16_t res = (mpu6050_read_reg(mpu, MPU_TEMP_OUT_H) << 8)
        | mpu6050_read_reg(mpu, MPU_TEMP_OUT_L);

    mpu->data_ready = false;
    return res;
}

bool mpu6050_has_data(MPU6050* mpu) {
    return mpu6050_read_reg(mpu, MPU_INT_STATUS) & MPU_INT_DATA_RDY_EN;
}
uint8_t mpu6050_get_addr(MPU6050* mpu) {
    return mpu6050_read_reg(mpu, MPU_WHO_AM_I);
}

void mpu6050_write_reg(MPU6050* mpu, uint8_t reg, uint8_t value) {
    uint8_t bytes[2] = { reg, value };
    if (i2c_is_sw(mpu->i2c)) {
        i2c_sw_write(mpu->i2c, 2, bytes);
    } else {
        i2c_send_data(mpu->i2c, 2, bytes);
    }
}
uint8_t mpu6050_read_reg(MPU6050* mpu, uint8_t reg) {
    uint8_t res;
    if (i2c_is_sw(mpu->i2c)) {
        i2c_sw_write(mpu->i2c, 1, &reg);
        i2c_sw_read(mpu->i2c, 1, &res);
    } else {
        i2c_send_data(mpu->i2c, 1, &reg);
        i2c_receive_data(mpu->i2c, 1, &res);
    }
    return res;
}

void mpu6050_isr(MPU6050* mpu) {
    // assumes gpio interrupt already detected
    if (gpio_event_detected(mpu->int_pin)) {
        mpu->data_ready = true;
        gpio_event_clear(mpu->int_pin);
    }
}

void mpu6050_self_test(MPU6050 *mpu)  {
    mpu6050_write_reg(mpu, MPU_ACCEL_CONFIG, MPU_ACCEL_FS_8G);
    mpu6050_write_reg(mpu, MPU_GYRO_CONFIG, MPU_GYRO_FS_250DPS);
    sys_timer_delay_ms(250);

    int16_t accel_x = mpu6050_read_accel_x_raw(mpu);
    int16_t accel_y = mpu6050_read_accel_y_raw(mpu);
    int16_t accel_z = mpu6050_read_accel_z_raw(mpu);
    int16_t gyro_x = mpu6050_read_gyro_x_raw(mpu);
    int16_t gyro_y = mpu6050_read_gyro_y_raw(mpu);
    int16_t gyro_z = mpu6050_read_gyro_z_raw(mpu);

    mpu6050_write_reg(mpu, MPU_ACCEL_CONFIG, MPU_ACCEL_FS_8G | 0b111 << 5);
    mpu6050_write_reg(mpu, MPU_GYRO_CONFIG, MPU_GYRO_FS_250DPS | 0b111 << 5);
    sys_timer_delay_ms(250);

    int16_t accel_x_st = mpu6050_read_accel_x_raw(mpu);
    int16_t accel_y_st = mpu6050_read_accel_y_raw(mpu);
    int16_t accel_z_st = mpu6050_read_accel_z_raw(mpu);
    int16_t gyro_x_st = mpu6050_read_gyro_x_raw(mpu);
    int16_t gyro_y_st = mpu6050_read_gyro_y_raw(mpu);
    int16_t gyro_z_st = mpu6050_read_gyro_z_raw(mpu);

    uint8_t test_x = mpu6050_read_reg(mpu, MPU_SELF_TEST_X);
    uint8_t test_y = mpu6050_read_reg(mpu, MPU_SELF_TEST_Y);
    uint8_t test_z = mpu6050_read_reg(mpu, MPU_SELF_TEST_Z);
    uint8_t test_a = mpu6050_read_reg(mpu, MPU_SELF_TEST_A);

    uint8_t xg_test = test_x & 0x1F;
    uint8_t yg_test = test_y & 0x1F;
    uint8_t zg_test = test_z & 0x1F;
    uint8_t xa_test = ((test_x >> 5) << 2) | ((test_a >> 4) & 0b11);
    uint8_t ya_test = ((test_y >> 5) << 2) | (test_a >> 2 & 0b11);
    uint8_t za_test = ((test_z >> 5) << 2) | (test_a & 0b11);

    float xg_ft = xg_test == 0 ? 0 : 25 * 131 * powf(1.046f, xg_test - 1);
    float yg_ft = yg_test == 0 ? 0 : -25 * 131 * powf(1.046f, yg_test - 1);
    float zg_ft = zg_test == 0 ? 0 : 25 * 131 * powf(1.046f, zg_test - 1);

    float xa_ft = xa_test == 0 ? 0 : 4096 * 0.34f * powf(0.92f/0.34f, (xa_test - 1)/30.f);
    float ya_ft = ya_test == 0 ? 0 : 4096 * 0.34f * powf(0.92f/0.34f, (ya_test - 1)/30.f);
    float za_ft = za_test == 0 ? 0 : 4096 * 0.34f * powf(0.92f/0.34f, (za_test - 1)/30.f);

    printk("Factory Trim Values:\n");
    printk("Gyro X: %f\nGyro Y: %f\nGyro Z: %f\n", xg_ft, yg_ft, zg_ft);
    printk("Accel X: %f\nAccel Y: %f\nAccel Z: %f\n", xa_ft, ya_ft, za_ft);
    
    printk("Measurements before self-test:\n");
    printk("Gyro X: %d\nGyro Y: %d\nGyro Z: %d\n", gyro_x, gyro_y, gyro_z);
    printk("Accel X: %d\nAccel Y:%d\nAccel Z:%d\n", accel_x, accel_y, accel_z);

    printk("Measurements after self-test:\n");
    printk("Gyro X: %d\nGyro Y:%d\nGyro Z:%d\n", gyro_x_st, gyro_y_st, gyro_z_st);
    printk("Accel X: %d\nAccel Y:%d\nAccel Z:%d\n", accel_x_st, accel_y_st, accel_z_st);

    float xg_change = (gyro_x_st - gyro_x - xg_ft)/xg_ft;
    float yg_change = (gyro_y_st - gyro_y - yg_ft)/yg_ft;
    float zg_change = (gyro_z_st - gyro_z - zg_ft)/zg_ft;
    float xa_change = (accel_x_st - accel_x - xa_ft)/xa_ft;
    float ya_change = (accel_y_st - accel_y - ya_ft)/ya_ft;
    float za_change = (accel_z_st - accel_z - za_ft)/za_ft;

    printk("Change Normalized by Factory Trim:\n");
    printk("Gyro X: %f\nGyro Y:%f\nGyro Z:%f\n", xg_change * 100, yg_change * 100, zg_change * 100);
    printk("Accel X: %f\nAccel Y:%f\nAccel Z:%f\n", xa_change * 100, ya_change * 100, za_change * 100);

    mpu6050_write_reg(mpu, MPU_ACCEL_CONFIG, mpu->accel_sel);
    mpu6050_write_reg(mpu, MPU_GYRO_CONFIG, mpu->gyro_sel);
}
