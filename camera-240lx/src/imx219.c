#include "imx219.h"
#include "i2c.h"
#include "gpio.h"
#include "lib.h"
#include "sys_timer.h"

static I2C g_i2c;
static IMX219ModeInfo g_mode;
static bool g_powered = false;

typedef struct { uint16_t reg; uint8_t val; } RegVal;

static const RegVal regs_common[] = {
    {0x0100, 0x00}, // standby mode
    {0x30EB, 0x05}, {0x30EB, 0x0C},
    {0x300A, 0xFF}, {0x300B, 0xFF},
    {0x30EB, 0x05}, {0x30EB, 0x09}, // developer sequence to access special registers
    {0x0114, 0x01}, {0x0128, 0x00}, // CSI lane mode = 2, auto MIPI global timing
    {0x012A, 0x18}, {0x012B, 0x00}, // some clock setting
    {0}
};
static const RegVal regs_640x480[] = {
    {0x0164, 0x03}, {0x0165, 0xE8}, {0x0166, 0x08}, {0x0167, 0xE7}, // 1280 pixel wide address
    {0x0168, 0x02}, {0x0169, 0xF0}, {0x016A, 0x06}, {0x016B, 0xAF}, // 960 line tall address
    {0x016C, 0x02}, {0x016D, 0x80}, {0x016E, 0x01}, {0x016F, 0xE0}, // 640x480 output
    {0x0170, 0x01}, {0x0171, 0x01}, {0x0174, 0x03}, {0x0175, 0x03}, // 1 stride, 2x2 analog binning 
    {0x0301, 0x05}, {0x0303, 0x01}, {0x0304, 0x03},
    {0x0305, 0x03}, {0x0306, 0x00}, {0x0307, 0x39},
    {0x030B, 0x01}, {0x030C, 0x00}, {0x030D, 0x72}, // clock stuff
    {0x0624, 0x06}, {0x0625, 0x68}, {0x0626, 0x04}, {0x0627, 0xD0}, // test pattern window settings
    {0x455E, 0x00}, {0x471E, 0x4B}, {0x4767, 0x0F}, {0x4750, 0x14},
    {0x4540, 0x00}, {0x47B4, 0x14}, {0x4713, 0x30}, {0x478B, 0x10},
    {0x478F, 0x10}, {0x4793, 0x10}, {0x4797, 0x0E}, {0x479B, 0x0E}, // magic registers
    {0}
};
static const RegVal regs_1920x1080[] = {
    {0x0164, 0x02}, {0x0165, 0xA8}, {0x0166, 0x0A}, {0x0167, 0x27},
    {0x0168, 0x02}, {0x0169, 0xB4}, {0x016A, 0x06}, {0x016B, 0xEB},
    {0x016C, 0x07}, {0x016D, 0x80}, {0x016E, 0x04}, {0x016F, 0x38},
    {0x0170, 0x01}, {0x0171, 0x01}, {0x0174, 0x00}, {0x0175, 0x00},
    {0x0301, 0x05}, {0x0303, 0x01}, {0x0304, 0x03}, {0x0305, 0x03},
    {0x0306, 0x00}, {0x0307, 0x39}, {0x030B, 0x01}, {0x030C, 0x00},
    {0x030D, 0x72}, {0x0624, 0x07}, {0x0625, 0x80}, {0x0626, 0x04},
    {0x0627, 0x38}, {0x455E, 0x00}, {0x471E, 0x4B}, {0x4767, 0x0F},
    {0x4750, 0x14}, {0x4540, 0x00}, {0x47B4, 0x14}, {0x4713, 0x30},
    {0x478B, 0x10}, {0x478F, 0x10}, {0x4793, 0x10}, {0x4797, 0x0E},
    {0x479B, 0x0E}, {0}
};
static const RegVal regs_raw8[] = {
    {0x018C, 0x08}, {0x018D, 0x08}, {0x0309, 0x08}, {0}
};
static const RegVal regs_raw10[] = {
    {0x018C, 0x0A}, {0x018D, 0x0A}, {0x0309, 0x0A}, {0}
};

static bool read_reg(uint16_t reg, uint8_t* val) {
    uint8_t buf[2] = { reg >> 8, reg & 0xFF };
    if (i2c_send_data(&g_i2c, 2, buf) != I2C_RESULT_OK) return false;
    if (i2c_receive_data(&g_i2c, 1, val) != I2C_RESULT_OK) return false;
    return true;
}
static bool write_reg(uint16_t reg, uint8_t val) {
    uint8_t buf[3] = { reg >> 8, reg & 0xFF, val };
    return i2c_send_data(&g_i2c, 3, buf) == I2C_RESULT_OK;
}
static bool write_reg16(uint16_t reg, uint16_t val) {
    uint8_t buf[4] = { reg >> 8, reg & 0xFF, val >> 8, val & 0xFF };
    return i2c_send_data(&g_i2c, 4, buf) == I2C_RESULT_OK;
}
static bool read_reg16(uint16_t reg, uint16_t* val) {
    uint8_t buf[2] = { reg >> 8, reg & 0xFF };
    if (i2c_send_data(&g_i2c, 2, buf) != I2C_RESULT_OK) return false;
    if (i2c_receive_data(&g_i2c, 2, buf) != I2C_RESULT_OK) return false;
    *val = (buf[0] << 8) | buf[1];
    return true;
}

static bool write_regs(const RegVal* regs) {
    for (; regs->reg; regs++) {
        if (!write_reg(regs->reg, regs->val)) return false;
    }
    return true;
}

static void set_cam_power(bool on) {
    const Pin pwr_pin = { IMX219_PWR_PIN };
    gpio_select(pwr_pin, GPIO_OUTPUT);
    if (on) {
        gpio_set_high(pwr_pin);
    } else {
        gpio_set_low(pwr_pin);
    }
    sys_timer_delay_us(100);

    PinOutput level = gpio_read(pwr_pin);
    printk("imx219: GPIO %d set to %s, read back=%d\n", IMX219_PWR_PIN, on ? "HIGH" : "LOW", level);
}

bool imx219_init() {
    set_cam_power(true);
    g_powered = true;
    sys_timer_delay_us(6200);
    
    g_i2c.bsc = BSC0;
    g_i2c.sda = (Pin) { IMX219_I2C_SDA };
    g_i2c.scl = (Pin) { IMX219_I2C_SCL };
    g_i2c.speed_hz = IMX219_I2C_HZ;
    g_i2c.slave_addr = IMX219_I2C_ADDR;
    
    gpio_set_pull(g_i2c.sda, GPIO_PULL_UP);
    gpio_set_pull(g_i2c.scl, GPIO_PULL_UP);
    i2c_init(&g_i2c);

    return imx219_probe();
}

void imx219_deinit() {
    if (g_powered) {
        write_reg(IMX219_MODE_SELECT, IMX219_MODE_STANDBY);
        set_cam_power(false);
        g_powered = false;
    }
}

bool imx219_probe() {
    uint16_t chip_id;
    if (!read_reg16(IMX219_CHIP_ID_REG, &chip_id)) return false;
    if (chip_id != IMX219_CHIP_ID) return false;
    
    write_reg(IMX219_MODE_SELECT, IMX219_MODE_STREAMING);
    sys_timer_delay_us(100);
    write_reg(IMX219_MODE_SELECT, IMX219_MODE_STANDBY);
    sys_timer_delay_us(100);
    
    return true;
}

bool imx219_set_mode(IMX219Mode mode, uint8_t depth) {
    write_reg(IMX219_MODE_SELECT, IMX219_MODE_STANDBY);
    
    uint8_t val;
    read_reg(IMX219_MODE_SELECT, &val);
    printk("imx219: mode_select after standby = %x (expect 0)\n", val);

    const RegVal* mode_regs;
    switch (mode) {
        case IMX219_MODE_1920x1080:
            mode_regs = regs_1920x1080;
            g_mode = (IMX219ModeInfo) { 1920, 1080, 0x6E3 };
            break;
        case IMX219_MODE_640x480:
        default:
            mode_regs = regs_640x480;
            g_mode = (IMX219ModeInfo) { 640, 480, 0x6E3 };
            break;
    }

    if (!write_regs(regs_common)) return false;
    if (!write_regs(mode_regs)) return false;
    if (!write_regs(depth == 10 ? regs_raw10 : regs_raw8)) return false;

    // Set VBlank and HBlank (align with vts_def)
    if (!write_reg16(IMX219_VTS, g_mode.vts_def)) return false;
    if (!write_reg16(IMX219_HTS, IMX219_PPL_MIN)) return false;

    return true;
}

IMX219ModeInfo imx219_get_mode_info() {
    return g_mode;
}

bool imx219_start_streaming() {
    if (!write_reg(IMX219_MODE_SELECT, IMX219_MODE_STREAMING)) {
        printk("imx219: failed to start streaming\n");
        return false;
    }
    sys_timer_delay_us(50000);
    
    uint16_t frame_len, line_len;
    read_reg16(IMX219_VTS, &frame_len);
    read_reg16(IMX219_HTS, &line_len);
    printk("imx219: frame_len=%d, line_len=%d\n", frame_len, line_len);
    
    uint16_t status = 0;
    read_reg16(IMX219_CHIP_ID_REG, &status);
    printk("imx219: chip_id=%x (should be 0x0219)\n", status);
    
    uint8_t mode = 0;
    read_reg(IMX219_MODE_SELECT, &mode);
    printk("imx219: mode_select=%x (1=streaming)\n", mode);

    uint16_t min_expos, max_expos_margin;
    read_reg16(0x1004, &min_expos);
    read_reg16(0x1006, &max_expos_margin);
    printk("min expos: %d\nmax expos margin: %d\n", min_expos, max_expos_margin);
    
    uint8_t frame_cnt;
    read_reg(0x0018, &frame_cnt);
    printk("frame cnt: %d\n", frame_cnt);

    uint16_t exposure;
    uint8_t gain;
    read_reg16(IMX219_EXPOSURE, &exposure);
    read_reg(IMX219_ANALOG_GAIN, &gain);
    printk("exposure: %d\ngain: %d\n", exposure, gain);

    return true;
}

void imx219_stop_streaming() {
    write_reg(IMX219_MODE_SELECT, IMX219_MODE_STANDBY);
}

bool imx219_set_exposure(uint16_t lines) {
    return write_reg16(IMX219_EXPOSURE, lines);
}
bool imx219_set_gain(uint8_t gain) {
    if (gain > IMX219_GAIN_MAX) gain = IMX219_GAIN_MAX;
    return write_reg(IMX219_ANALOG_GAIN, gain);
}
bool imx219_set_digital_gain(uint16_t gain) {
    if (gain < IMX219_DGAIN_MIN) gain = IMX219_DGAIN_MIN;
    if (gain > IMX219_DGAIN_MAX) gain = IMX219_DGAIN_MAX;
    return write_reg16(IMX219_DIGITAL_GAIN, gain);
}
bool imx219_set_vflip(bool enable) {
    uint8_t val;
    if (!read_reg(IMX219_ORIENTATION, &val)) return false;
    val = enable ? (val | 2) : (val & ~2);
    return write_reg(IMX219_ORIENTATION, val);
}
bool imx219_set_hflip(bool enable) {
    uint8_t val;
    if (!read_reg(IMX219_ORIENTATION, &val)) return false;
    val = enable ? (val | 1) : (val & ~1);
    return write_reg(IMX219_ORIENTATION, val);
}

uint16_t imx219_get_HTS() {
    uint16_t res;
    if (!read_reg16(IMX219_HTS, &res)) return 0;
    return res;
}
uint16_t imx219_get_exposure() {
    uint16_t res;
    if (!read_reg16(IMX219_EXPOSURE, &res)) return 0;
    return res;
}
uint8_t imx219_get_analog_gain() {
    uint8_t res;
    if (!read_reg(IMX219_ANALOG_GAIN, &res)) return 0;
    return res;
}
uint16_t imx219_get_digital_gain() {
    uint16_t res;
    if (!read_reg16(IMX219_DIGITAL_GAIN, &res)) return 0;
    return res;
}
