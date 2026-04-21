#ifndef MPU6050_H
#define MPU6050_H

#include "i2c.h"

#define MPU6050_ADDR 0x68

enum MPU6050_Regs {
    MPU_SELF_TEST_X        = 0x0D,
    MPU_SELF_TEST_Y        = 0x0E,
    MPU_SELF_TEST_Z        = 0x0F,
    MPU_SELF_TEST_A        = 0x10,

    MPU_SMPLRT_DIV         = 0x19,
    MPU_CONFIG             = 0x1A,
    MPU_GYRO_CONFIG        = 0x1B,
    MPU_ACCEL_CONFIG       = 0x1C,

    MPU_FIFO_EN            = 0x23,

    MPU_INT_PIN_CFG        = 0x37,
    MPU_INT_ENABLE         = 0x38,
    MPU_INT_STATUS         = 0x3A,

    MPU_ACCEL_XOUT_H       = 0x3B,
    MPU_ACCEL_XOUT_L       = 0x3C,
    MPU_ACCEL_YOUT_H       = 0x3D,
    MPU_ACCEL_YOUT_L       = 0x3E,
    MPU_ACCEL_ZOUT_H       = 0x3F,
    MPU_ACCEL_ZOUT_L       = 0x40,

    MPU_TEMP_OUT_H         = 0x41,
    MPU_TEMP_OUT_L         = 0x42,

    MPU_GYRO_XOUT_H        = 0x43,
    MPU_GYRO_XOUT_L        = 0x44,
    MPU_GYRO_YOUT_H        = 0x45,
    MPU_GYRO_YOUT_L        = 0x46,
    MPU_GYRO_ZOUT_H        = 0x47,
    MPU_GYRO_ZOUT_L        = 0x48,

    MPU_SIGNAL_PATH_RESET  = 0x68,
    MPU_USER_CTRL          = 0x6A,
    MPU_PWR_MGMT_1         = 0x6B,
    MPU_PWR_MGMT_2         = 0x6C,

    MPU_FIFO_COUNTH        = 0x72,
    MPU_FIFO_COUNTL        = 0x73,
    MPU_FIFO_R_W           = 0x74,
    MPU_WHO_AM_I           = 0x75
};

enum MPU6050_USER_CTRL_BITS {
    MPU_USER_CTRL_SIG_COND_RESET = 1 << 0,
    MPU_USER_CTRL_I2C_MST_RESET  = 1 << 1,
    MPU_USER_CTRL_FIFO_RESET     = 1 << 2,
    MPU_USER_CTRL_I2C_IF_DIS     = 1 << 4,
    MPU_USER_CTRL_I2C_MST_EN     = 1 << 5,
    MPU_USER_CTRL_FIFO_EN        = 1 << 6
};

enum MPU6050_PWR_MGMT_1 {
    MPU_PWR1_CLKSEL_INTERNAL  = 0x00,
    MPU_PWR1_CLKSEL_PLL_XGYRO = 0x01,
    MPU_PWR1_CLKSEL_PLL_YGYRO = 0x02,
    MPU_PWR1_CLKSEL_PLL_ZGYRO = 0x03,
    MPU_PWR1_CLKSEL_PLL_EXT32K= 0x04,
    MPU_PWR1_CLKSEL_PLL_EXT19M= 0x05,
    MPU_PWR1_CLKSEL_STOP      = 0x07,

    MPU_PWR1_TEMP_DIS         = 1 << 3,
    MPU_PWR1_CYCLE            = 1 << 5,
    MPU_PWR1_SLEEP            = 1 << 6,
    MPU_PWR1_DEVICE_RESET     = 1 << 7 
};

enum MPU6050_INT_ENABLE {
    MPU_INT_DATA_RDY_EN   = 1 << 0,
    MPU_INT_DMP_EN        = 1 << 1,
    MPU_INT_FIFO_OFLOW_EN = 1 << 4,
    MPU_INT_I2C_MST_EN    = 1 << 3,
    MPU_INT_LATCH_EN      = 1 << 5,
};

typedef enum {
    MPU_DLPF_260HZ = 0, // Accel 260Hz / Gyro 256Hz
    MPU_DLPF_184HZ = 1,
    MPU_DLPF_94HZ  = 2,
    MPU_DLPF_44HZ  = 3,
    MPU_DLPF_21HZ  = 4,
    MPU_DLPF_10HZ  = 5,
    MPU_DLPF_5HZ   = 6,
    MPU_DLPF_OFF   = 7
}  MPU6050_DLPF;

typedef enum {
    MPU_GYRO_FS_250DPS  = (0 << 3),
    MPU_GYRO_FS_500DPS  = (1 << 3),
    MPU_GYRO_FS_1000DPS = (2 << 3),
    MPU_GYRO_FS_2000DPS = (3 << 3)
} MPU6050_Gyro_Sel;

typedef enum {
    MPU_ACCEL_FS_2G  = (0 << 3),
    MPU_ACCEL_FS_4G  = (1 << 3),
    MPU_ACCEL_FS_8G  = (2 << 3),
    MPU_ACCEL_FS_16G = (3 << 3)
} MPU6050_Accel_Sel;

enum MPU6050_FIFO_EN_BITS {
    MPU_FIFO_EN_SLV0   = 1 << 0,
    MPU_FIFO_EN_SLV1   = 1 << 1,
    MPU_FIFO_EN_SLV2   = 1 << 2,

    MPU_FIFO_EN_ACCEL  = 1 << 3,

    MPU_FIFO_EN_GYRO_Z = 1 << 4,
    MPU_FIFO_EN_GYRO_Y = 1 << 5,
    MPU_FIFO_EN_GYRO_X = 1 << 6,

    MPU_FIFO_EN_TEMP   = 1 << 7
};

typedef struct {
    I2C* i2c;
    Pin int_pin;

    MPU6050_DLPF dlpf;
    MPU6050_Gyro_Sel gyro_sel;
    MPU6050_Accel_Sel accel_sel;
    
    volatile bool data_ready;
} MPU6050;

int16_t mpu6050_read_gyro_x_raw(MPU6050* mpu);
int16_t mpu6050_read_gyro_y_raw(MPU6050* mpu);
int16_t mpu6050_read_gyro_z_raw(MPU6050* mpu);
float mpu6050_read_gyro_x(MPU6050* mpu);
float mpu6050_read_gyro_y(MPU6050* mpu);
float mpu6050_read_gyro_z(MPU6050* mpu);

int16_t mpu6050_read_accel_x_raw(MPU6050* mpu);
int16_t mpu6050_read_accel_y_raw(MPU6050* mpu);
int16_t mpu6050_read_accel_z_raw(MPU6050* mpu);
float mpu6050_read_accel_x(MPU6050* mpu);
float mpu6050_read_accel_y(MPU6050* mpu);
float mpu6050_read_accel_z(MPU6050* mpu);

int16_t mpu6050_read_temp(MPU6050* mpu);

void mpu6050_config(MPU6050* mpu);
void mpu6050_reset(MPU6050* mpu);
bool mpu6050_has_data(MPU6050* mpu);
uint8_t mpu6050_get_addr(MPU6050 *mpu);

void mpu6050_write_reg(MPU6050 *mpu, uint8_t reg, uint8_t value);
uint8_t mpu6050_read_reg(MPU6050 *mpu, uint8_t reg);

void mpu6050_isr(MPU6050* mpu);

void mpu6050_self_test(MPU6050 *mpu);

#endif
