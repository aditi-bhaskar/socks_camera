#ifndef IMX219_H
#define IMX219_H

#include <stdint.h>
#include <stdbool.h>

#define IMX219_CHIP_ID          0x0219
#define IMX219_PIXEL_RATE_MHZ   ((float) 182.4)

// Registers
#define IMX219_MODE_SELECT      0x0100
#define IMX219_CHIP_ID_REG      0x0000
#define IMX219_VTS              0x0160
#define IMX219_HTS              0x0162
#define IMX219_EXPOSURE         0x015A
#define IMX219_ANALOG_GAIN      0x0157
#define IMX219_DIGITAL_GAIN     0x0158
#define IMX219_ORIENTATION      0x0172
#define IMX219_TEST_PATTERN     0x0600

// HBlank control ranges
#define IMX219_PPL_MIN          3448

// Mode values
#define IMX219_MODE_STANDBY     0x00
#define IMX219_MODE_STREAMING   0x01

// Sensor limits
#define IMX219_EXPOSURE_MIN     4
#define IMX219_EXPOSURE_MAX     65535
#define IMX219_GAIN_MIN         0
#define IMX219_GAIN_MAX         232
#define IMX219_DGAIN_MIN        0x100
#define IMX219_DGAIN_MAX        0xFFF

// Pins + I2C
#define IMX219_PWR_PIN          44
#define IMX219_I2C_ADDR         0x10
#define IMX219_I2C_SDA          28
#define IMX219_I2C_SCL          29
#define IMX219_I2C_HZ           100000

typedef enum {
    IMX219_MODE_640x480,
    IMX219_MODE_1640x1232,
    IMX219_MODE_1920x1080,
    IMX219_MODE_3280x2464,
} IMX219Mode;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t vts_def;
} IMX219ModeInfo;

bool imx219_init();
void imx219_deinit();
bool imx219_probe();
bool imx219_set_mode(IMX219Mode mode, uint8_t depth);
IMX219ModeInfo imx219_get_mode_info();
bool imx219_start_streaming();
void imx219_stop_streaming();

bool imx219_set_exposure(uint16_t lines);
bool imx219_set_gain(uint8_t gain);
bool imx219_set_digital_gain(uint16_t gain);
bool imx219_set_vflip(bool enable);
bool imx219_set_hflip(bool enable);

uint16_t imx219_get_HTS();
uint16_t imx219_get_exposure();
uint8_t imx219_get_analog_gain();
uint16_t imx219_get_digital_gain();

#endif
