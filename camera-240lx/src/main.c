


// a big thank you to anna and justin for some starter code for the camera driver!

#include "lib.h"
#include "camera.h"
// #include "display.h"
#include "mmu.h"
#include "sys_timer.h"
#include "ssd1306-display-driver.h"
#include "drivers.h"

#define FB_WIDTH  640
#define FB_HEIGHT 480
// #define FB_WIDTH  1920
// #define FB_HEIGHT 1080

#define N 4

#define BUTTON_PIN_A 15
#define BUTTON_PIN_B 26


static CameraBuffer bufs[N];

// Bilinear debayer for 16-bit RGGB Bayer pattern
static void debayer(uint16_t* bayer, uint32_t* rgb, uint32_t w, uint32_t h, 
                    uint32_t in_stride_bytes, uint32_t out_stride_pixels) {
    int32_t s = in_stride_bytes / 2;
    
    // uint32_t ravg = 0;
    // uint32_t gavg = 0;
    // uint32_t bavg = 0;
    
    for (uint32_t y = 1; y < h - 1; y++) {
        for (uint32_t x = 1; x < w - 1; x++) {
            uint16_t* p = bayer + y * s + x;

            uint32_t r, g, b;
            if ((y & 1) == 0) {
                if ((x & 1) == 0) {
                    r = p[0];
                    g = (p[-1] + p[1] + p[-s] + p[s]) >> 2;
                    b = (p[-s - 1] + p[-s + 1] + p[s - 1] + p[s + 1]) >> 2;
                } else {
                    r = (p[-1] + p[1]) >> 1;
                    g = p[0];
                    b = (p[-s] + p[s]) >> 1;
                }
            } else {
                if ((x & 1) == 0) {
                    r = (p[-s] + p[s]) >> 1;
                    g = p[0];
                    b = (p[-1] + p[1]) >> 1;
                } else {
                    r = (p[-s - 1] + p[-s + 1] + p[s - 1] + p[s + 1]) >> 2;
                    g = (p[-1] + p[1] + p[-s] + p[s]) >> 2;
                    b = p[0];
                }
            }

            r = r >> 2;
            g = g >> 2;
            b = b >> 2;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            rgb[y * out_stride_pixels + x] = (r << 16) | (g << 8) | b;
        }
    }
}

static uint8_t gray_buf[FB_WIDTH * FB_HEIGHT];

static void edge_detect(uint32_t* rgb, uint32_t w, uint32_t h) {
    // 3x3 box blur + grayscale combined: suppresses sensor noise before Sobel
    for (uint32_t y = 1; y < h - 1; y++) {
        for (uint32_t x = 1; x < w - 1; x++) {
            uint32_t sum = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t p = rgb[(y + dy) * w + (x + dx)];
                    sum += ((p >> 16) & 0xFF) + ((p >> 8) & 0xFF) + (p & 0xFF);
                }
            }
            gray_buf[y * w + x] = sum / 27; // 9 pixels * 3 channels
        }
    }

    for (uint32_t y = 1; y < h - 1; y++) {
        for (uint32_t x = 1; x < w - 1; x++) {
            int gx = -  gray_buf[(y-1)*w + (x-1)] + gray_buf[(y-1)*w + (x+1)]
                     - 2*gray_buf[  y  *w + (x-1)] + 2*gray_buf[  y  *w + (x+1)]
                     -  gray_buf[(y+1)*w + (x-1)] + gray_buf[(y+1)*w + (x+1)];

            int gy = -  gray_buf[(y-1)*w + (x-1)] - 2*gray_buf[(y-1)*w +  x  ] - gray_buf[(y-1)*w + (x+1)]
                     +  gray_buf[(y+1)*w + (x-1)] + 2*gray_buf[(y+1)*w +  x  ] + gray_buf[(y+1)*w + (x+1)];

            int mag = ((gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy)) * 4;
            if (mag > 255) mag = 255;
            rgb[y*w + x] = (mag << 16) | (mag << 8) | mag;
        }
    }

    for (uint32_t x = 0; x < w; x++) { rgb[x] = 0; rgb[(h-1)*w + x] = 0; }
    for (uint32_t y = 0; y < h; y++) { rgb[y*w] = 0; rgb[y*w + (w-1)] = 0; }
}

void downsample_to_oled(uint32_t* rgb, uint32_t rgb_w, uint32_t rgb_h) {
    for (uint32_t oy = 0; oy < SSD1306_DISPLAY_HEIGHT; oy++) {
        for (uint32_t ox = 0; ox < SSD1306_DISPLAY_WIDTH; ox++) {
            uint32_t sx = ox * rgb_w / SSD1306_DISPLAY_WIDTH;
            uint32_t sy = oy * rgb_h / SSD1306_DISPLAY_HEIGHT;
            uint32_t pixel = rgb[sy * rgb_w + sx];

            // extract RGB
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >>  8) & 0xFF;
            uint8_t b = (pixel      ) & 0xFF;

            // luminance (simple average, or weighted)
            uint8_t luma = (r + g + b) / 3;

            // threshold to 1-bit
            color_t color = (luma > 30) ? COLOR_WHITE : COLOR_BLACK;
            ssd1306_display_draw_pixel(ox, oy, color);
        }
    }
}

#define FRAME_MAGIC 0x42424242

void uart_send_word(uint32_t word) {
    for (int i = 24; i >= 0; i -= 8) {
        uart_putc((uint8_t)((word >> i) & 0xFF));
    }
}


void uart_send_frame(uint32_t* rgb_buf, uint32_t w, uint32_t h) {
    // Header: magic + dimensions
    uart_send_word(FRAME_MAGIC);
    uart_send_word(w);
    uart_send_word(h);
    

    // Send pixels — to save bandwidth, convert to 8-bit grayscale
    // since your edge_detect output is already grayscale
    for (uint32_t i = 0; i < w * h; i++) {
        uint8_t r = (uint8_t)((rgb_buf[i] >> 16) & 0xFF);
        uint8_t g = (uint8_t)((rgb_buf[i] >> 8) & 0xFF);
        uint8_t b = (uint8_t)((rgb_buf[i] >> 0) & 0xFF);
        uart_putc(r);
        uart_putc(g);
        uart_putc(b);
    }
}

// Tune these multipliers until the image looks neutral
#define WB_R  1.8f
#define WB_G  1.3f   // boost green back up a bit
#define WB_B  1.1f   // bring blue way down

void apply_white_balance(uint32_t* rgb, uint32_t w, uint32_t h) {
    for (uint32_t i = 0; i < w * h; i++) {
        uint32_t r = (rgb[i] >> 16) & 0xFF;
        uint32_t g = (rgb[i] >>  8) & 0xFF;
        uint32_t b = (rgb[i]      ) & 0xFF;

        r = (uint32_t)(r * WB_R); if (r > 255) r = 255;
        g = (uint32_t)(g * WB_G); if (g > 255) g = 255;
        b = (uint32_t)(b * WB_B); if (b > 255) b = 255;

        rgb[i] = (r << 16) | (g << 8) | b;
    }
}

void uart_send_raw_frame(uint32_t* rgb_buf, uint32_t w, uint32_t h) {
    uart_send_word(FRAME_MAGIC);
    uart_send_word(w);
    uart_send_word(h);
    for (uint32_t i = 0; i < w * h; i++) {
        uart_putc((uint8_t)((rgb_buf[i] >> 16) & 0xFF));  // R
        uart_putc((uint8_t)((rgb_buf[i] >>  8) & 0xFF));  // G
        uart_putc((uint8_t)((rgb_buf[i]      ) & 0xFF));  // B
    }
}

void main() {
    uart_init();
    
    mmu_enable_caches();

    printk("jankencamera starting...\n");

    printk("IN THE MAIN.C FILE RN\n");

    ssd1306_display_init();
    // if (!display_init(FB_WIDTH, FB_HEIGHT)) {
    //     printk("display init failed\n");
    //     return;
    // }


    if (!camera_init()) {
        printk("camera init failed\n");
        return;
    }

    if (!camera_set_format(FB_WIDTH, FB_HEIGHT, CAM_FMT_BAYER_10)) {
        printk("camera set format failed\n");
        return;
    }

    // you can play around with exposure/gain
    // exposure is measured in microseconds
    // gain is measured in a multipler (1x - 128x)

    camera_set_exposure(10000);
    // camera_set_exposure(60000);
    // camera_set_exposure(100000);

    // camera_set_gain(4.0);
    camera_set_gain(8.0);
    // camera_set_gain(128.0);

    if (!camera_start()) {
        printk("camera start failed\n");
        return;
    }

    CameraConfig cfg = camera_get_config();
    DisplayConfig disp = ssd1306_display_get_config();
    printk("streaming %dx%d\n", cfg.width, cfg.height);

    printk("configured camera \n");

    static uint32_t rgb_buf[640 * 480];
    CameraFrame frame;

    int i = 0;
    while (1) {
        if (camera_capture_frame(&frame)) {
            debayer((uint16_t*) frame.buf, rgb_buf,
                    cfg.width, cfg.height, cfg.stride, cfg.width);
            
            
            if (i==1000) {
                // apply_white_balance(rgb_buf, cfg.width, cfg.height);
                // uart_send_frame(rgb_buf, cfg.width, cfg.height);
                uart_send_raw_frame(rgb_buf, cfg.height, cfg.width);
                i=0;
            }
            edge_detect(rgb_buf, cfg.width, cfg.height); // claude thx
            downsample_to_oled(rgb_buf, cfg.width, cfg.height);
            ssd1306_display_show();
            i++;
        }
    }

    // CameraFrame frame;
    // while (1) {
    //     if (camera_capture_frame(&frame)) {
    //         // printk("ts: %d\n", sys_timer_get_usec());
    //         debayer((uint16_t*) frame.buf, display_get_buffer(),
    //                 cfg.width, cfg.height, cfg.stride, disp.pitch / 4);
    //         display_swap();
    //     }
    // }

    camera_stop();
}

