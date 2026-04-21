#include "rpi.h"
#include "ssd1306-display-driver.h"
#include "i2c.h"

static uint8_t i2c_buffer[SSD1306_I2C_BUFFER_SIZE];
static uint8_t *display_buffer = i2c_buffer + 1;

// Helper function to send a byte over I2C
void ssd1306_display_send_command(uint8_t cmd)
{
    uint8_t cmd_buf[2] = {0x00, cmd};
    i2c_write(SSD1306_DISPLAY_ADDRESS, cmd_buf, 2);
}

// Initialize the display.
// Requirement: I2C should have been initialized beforehand.
void ssd1306_display_init(void)
{
    /* Display initialization flow [SSD1306 datasheet pg 64] */

    // 0. Turn the display off to be safe [SSD1306 pg 28]
    ssd1306_display_send_command(0xAE);

    // 1. Set multiplex ratio
    ssd1306_display_send_command(0xA8);
    ssd1306_display_send_command(0x3F); // set to x3f = 63 so all 64 rows active

    // 2. Set display offset [SSD1306 pg 37]
    ssd1306_display_send_command(0xD3);
    ssd1306_display_send_command(0x00); // - set offset to 0

    // 3. Set display start line [SSD1306 pg 36]
    ssd1306_display_send_command(0x40); // - set to 0

    // 4. Set segment re-map [SSD1306 pg 36]
    ssd1306_display_send_command(0xA1); // remap column 0 to seg 0

    // 5. Set COM output scan direction
    ssd1306_display_send_command(0xC8); // set com63 to com0

    // 6. Set COM pins hardware configuration [SSD1306 pg 40]
    ssd1306_display_send_command(0xDA); //
    ssd1306_display_send_command(0x12);

    // 7. Set contrast control [SSD1306 pg 36]
    ssd1306_display_send_command(0x81);
    ssd1306_display_send_command(0xc0); // randomly chosen contrast between 00 and ff

    // 8. Display output according to GDDRAM contents [SSD1306 pg 37]
    ssd1306_display_send_command(0xA4);

    // 9. Set normal or inverse display [SSD1306 pg 37]
    ssd1306_display_send_command(0xA6);

    // 10. Set display clock divide ratio/oscillator frequency [SSD1306 pg 40]
    ssd1306_display_send_command(0xD5);
    ssd1306_display_send_command(0x80);

    // 11. Enable charge pump regulator [SSD1306 pg 62]
    ssd1306_display_send_command(0x8D);
    ssd1306_display_send_command(0x14);

    // 12. Specify HORIZONTAL addressing mode [SSD1306 pg 35]
    ssd1306_display_send_command(0x20);
    ssd1306_display_send_command(0x00); // horizontal mode

    // set col/page ranges too
    ssd1306_display_send_command(0x21); // set column address
    ssd1306_display_send_command(0x00); // COL[START] = 0
    ssd1306_display_send_command(0x7F); // COL[END] = 127

    ssd1306_display_send_command(0x22); // set page address
    ssd1306_display_send_command(0x00); // PAGE[START] = 0
    ssd1306_display_send_command(0x07); // PAGE[END] = 7

    // 13. Turn display on [SSD1306 pg 62]
    ssd1306_display_send_command(0xAF);

    // 14. Clear the screen to black and call display_show()
    ssd1306_display_clear();
    ssd1306_display_show();
}

// Send display buffer to screen via I2C
// Must be called to actually update the display!
void ssd1306_display_show(void)
{
    i2c_buffer[0] = 0x40; // control byte to indicate data
    i2c_write(SSD1306_DISPLAY_ADDRESS, i2c_buffer, sizeof(i2c_buffer));
}

// Clears the screen to black; no change until display_show() is called
void ssd1306_display_clear(void)
{
    i2c_buffer[0] = 0x40; // control byte to indicate data
    memset(display_buffer, 0, SSD1306_DISPLAY_BUFFER_SIZE);
}

// Fills the display completely with white
void ssd1306_display_fill_white(void)
{
    i2c_buffer[0] = 0x40; // control byte to indicate data
    memset(display_buffer, 0xFF, SSD1306_DISPLAY_BUFFER_SIZE);
}

void ssd1306_display_draw_pixel(uint16_t x, uint16_t y, color_t color)
{
    // https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp#L648
    // May need to perform additional coordinate transforms,
    // depending on what coordinate system you want to use with
    // the display.

    switch (color)
    {
    case COLOR_WHITE:
        display_buffer[(y / 8) * SSD1306_DISPLAY_WIDTH + x] |= (1 << (y & 7));
        break;
    case COLOR_BLACK:
        display_buffer[(y / 8) * SSD1306_DISPLAY_WIDTH + x] &= ~(1 << (y & 7));
        break;
    case COLOR_INVERT:
        display_buffer[(y / 8) * SSD1306_DISPLAY_WIDTH + x] ^= (1 << (y & 7));
        break;
    }
}

void ssd1306_display_draw_horizontal_line(int16_t x_start, int16_t x_end,
                                          int16_t y, color_t color)
{
    // https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp#L706
    if (y < 0 || y >= SSD1306_DISPLAY_HEIGHT) {
        return;
    }

    // Clamp x to display bounds
    x_start = MIN(0, x_start);
    x_end = MAX(SSD1306_DISPLAY_WIDTH-1, x_end);
    if (x_start > x_end) {
        SWAP(x_start, x_end);
    }

    uint8_t bit = 1 << (y & 7);
    uint8_t page = y / 8;

    for (int16_t x = x_start; x <= x_end; x++) {
        uint8_t *byte = &display_buffer[page * SSD1306_DISPLAY_WIDTH + x];
        *byte = (color == COLOR_WHITE) ?  *byte | bit
            : (color == COLOR_BLACK) ? *byte & ~bit
            : *byte ^ bit;
    }
}

void ssd1306_display_draw_vertical_line(int16_t y_start, int16_t y_end,
                                        int16_t x, color_t color)
{
    // https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp#L806
    if (x < 0 || x >= SSD1306_DISPLAY_WIDTH) {
        return;
    }

    // Clamp y to display bounds
    y_start = MAX(0, y_start);
    y_end = MIN(SSD1306_DISPLAY_HEIGHT-1, y_end);
    if (y_start > y_end) {
        SWAP(y_start, y_end);
    }

    for (int y = y_start; y <= y_end; y++) {
        uint8_t bit = 1 << (y & 7);
        uint8_t page = y / 8;
        uint8_t *byte = &display_buffer[page * SSD1306_DISPLAY_WIDTH + x];
        *byte = (color == COLOR_WHITE) ?  *byte | bit
            : (color == COLOR_BLACK) ? *byte & ~bit
            : *byte ^ bit;
    }

}

void ssd1306_display_draw_fill_rect(int16_t x, int16_t y, uint16_t w,
                                    uint16_t h, color_t color)
{

    // https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_GFX.cpp#L300
    for (int i = x; i < x + w; i++)
    {
        ssd1306_display_draw_vertical_line(y, y + h, i, color);
    }
}

void ssd1306_display_draw_character_size(uint16_t x, uint16_t y, unsigned char c, color_t color, uint8_t size_x, uint8_t size_y)
{
    // https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_GFX.cpp#L1249

    if ((x >= SSD1306_DISPLAY_WIDTH) ||  // Clip right
        (y >= SSD1306_DISPLAY_HEIGHT) || // Clip bottom
        ((x + 6 * size_x - 1) < 0) ||    // Clip left
        ((y + 8 * size_y - 1) < 0)) { // Clip top
        return;
    }

    // 5 bytes per character - 5 cols wide in ascii encoding + 1 for character spacing
    for (int col = 0; col < 6; col++) {
        int col_bits;

        if (col == 5) {
            col_bits = 0x00;  // 6th col for spacing
        } else {
            col_bits = pgm_read_byte(&standard_ascii_font[(c * 5) + col]); // take the bits from the font.c
        }
        for (int row = 0; row < 8; row++) {
            color_t pixel_color;

            if (col_bits & (1 << row)) {
                pixel_color = color;
            } else {
                // invert foreground
                pixel_color = (color == COLOR_WHITE) ? COLOR_BLACK
                            : (color == COLOR_BLACK) ? COLOR_WHITE
                            : COLOR_INVERT;
            }

            ssd1306_display_draw_fill_rect(x + col * size_x, y + row * size_y, size_x, size_y, pixel_color);
        }

    }


}
