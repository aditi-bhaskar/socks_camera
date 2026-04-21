// main_aditi.c
// this runs on aditi's pi: camera + OLED live view.
// button A cycles the view mode. button B sends the full frame over BT to
// christy (who forwards it to the laptop) and saves a downsampled copy to the SD card.

#include "lib.h"
#include "uart.h"
#include "camera.h"
#include "mmu.h"
#include "sys_timer.h"
#include "ssd1306-display-driver.h"
#include "bt.h"
#include "button.h"

// ported 140e FAT32 stack (SD card save)
#include "pi-sd.h"
#include "mbr.h"
#include "fat32.h"
#include "fat_heap.h"
#include "qoi.h"

#define FB_WIDTH  640
#define FB_HEIGHT 480

#define SEND_WIDTH  640
#define SEND_HEIGHT 480

// saved to SD downsampled so writes stay fast. the full-res frame still goes over BT.
#define SAVE_WIDTH  320
#define SAVE_HEIGHT 240

#define BUTTON_PIN_A 16   // cycle view mode
#define BUTTON_PIN_B 26   // send current frame over BT

// wire format. the laptop hunts for the 4-byte magic, then reads the 20-byte
// header MAGIC|W|H|FORMAT|PAYLOAD_LEN and PAYLOAD_LEN bytes. christy just forwards the bytes.
#define FRAME_MAGIC     0x42424242
#define FMT_RGB888 0   // PAYLOAD_LEN = w*h*3
#define FMT_RGB565 1   // PAYLOAD_LEN = w*h*2, big-endian 5:6:5

#define BT_LOCAL_NAME   "RPI Aditi"
#define BT_PEER_ADDR_BE { 0xb8, 0x27, 0xeb, 0x30, 0xd1, 0x77 }   // christy, big-endian
#define BT_BAUD         3000000
#define BT_PAGE_TIMEOUT 0xFFFF

static uint32_t rgb_buf[FB_WIDTH * FB_HEIGHT]; // working frame
static uint8_t gray_buf[FB_WIDTH * FB_HEIGHT]; // edge-detect scratch
static uint32_t send_buf[SEND_WIDTH * SEND_HEIGHT];  // frame packed for BT send

// ---- image pipeline ----

// the debayer algorithm, briefly, does the following:
// takes a raw 16-bit bayer-patterned image (RGGB layout) and reconstructs
// full RGB for each interior pixel via bilinear interpolation. the border
// (x=0, x=w-1, y=0, y=h-1) is skipped and left uninitialized.
//
// each pixel only captures one color channel natively — which one depends
// on its position parity. the other two are interpolated from neighbors:
//
//   even row, even col  →  R site: R=self, G=avg(N/S/E/W), B=avg(diagonals)
//   even row, odd col   →  G site (R row): R=avg(E/W), G=self, B=avg(N/S)
//   odd row,  even col  →  G site (B row): R=avg(N/S), G=self, B=avg(E/W)
//   odd row,  odd col   →  B site: R=avg(diagonals), G=avg(N/S/E/W), B=self
//
// after interpolation, values are right-shifted by 2 (loosely 16->14 bit),
// then scaled by per-channel white balance gains in 1/256 fixed-point
// (256 = 1.0x), and clamped to [0,255]. packed into 0x00RRGGBB output. 
static void debayer(uint16_t *bayer, uint32_t *rgb, uint32_t w, uint32_t h,
             uint32_t in_stride_bytes, uint32_t out_stride_pixels) {
    int32_t s = in_stride_bytes / 2;
    for (uint32_t y = 1; y < h - 1; y++) {
        for (uint32_t x = 1; x < w - 1; x++) {
            uint16_t *p = bayer + y * s + x;
            uint32_t r, g, b;
            if ((y & 1) == 0) {
                if ((x & 1) == 0) {
                    r = p[0];
                    g = (p[-1] + p[1] + p[-s] + p[s]) >> 2;
                    b = (p[-s-1] + p[-s+1] + p[s-1] + p[s+1]) >> 2;
                } else {
                    r = (p[-1] + p[1]) >> 1; g = p[0]; b = (p[-s] + p[s]) >> 1;
                }
            } else {
                if ((x & 1) == 0) {
                    r = (p[-s] + p[s]) >> 1; g = p[0]; b = (p[-1] + p[1]) >> 1;
                } else {
                    r = (p[-s-1] + p[-s+1] + p[s-1] + p[s+1]) >> 2;
                    g = (p[-1] + p[1] + p[-s] + p[s]) >> 2;
                    b = p[0];
                }
            }
            r >>= 2; g >>= 2; b >>= 2;
            // white balance gains, 1/256 units (256 = 1.0x)
            #define WB_R_GAIN 555
            #define WB_G_GAIN 384
            #define WB_B_GAIN 571
            r = (r * WB_R_GAIN) >> 8;
            g = (g * WB_G_GAIN) >> 8;
            b = (b * WB_B_GAIN) >> 8;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            rgb[y * out_stride_pixels + x] = (r << 16) | (g << 8) | b;
        }
    }
}

// the edge detection algo, briefly, does the following:
// pass 1: converts each interior pixel to grayscale by averaging R+G+B
//   across its full 3x3 neighborhood (sum/27) into gray_buf.
// pass 2: runs sobel [gx, gy] on gray_buf, approximates magnitude as
//   |gx|+|gy|, thresholds noise at 40 (zero below, 2x above), clamps to
//   [0,255], writes back as (mag,mag,mag) — edges are white on black.
// border fixup: explicitly zeroes the 1-pixel border skipped by both passes.
//
// notably, we downsample_to_oled: 
// we use nearest-neighbor which scales rgb down to SSD1306 dims,
//   threshold for luma is 80 for 1-bit black/white (display can only show b or w)
static void edge_detect(uint32_t *rgb, uint32_t w, uint32_t h) {
    for (uint32_t y = 1; y < h - 1; y++){
        for (uint32_t x = 1; x < w - 1; x++) {
            uint32_t sum = 0;
            for (int dy = -1; dy <= 1; dy++){
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t p = rgb[(y+dy)*w + (x+dx)];
                    sum += ((p>>16)&0xFF) + ((p>>8)&0xFF) + (p&0xFF);
                }
            }
            gray_buf[y*w + x] = sum / 27;
        }
    }
    for (uint32_t y = 1; y < h - 1; y++){
        for (uint32_t x = 1; x < w - 1; x++) {
            int gx = - gray_buf[(y-1)*w+(x-1)] + gray_buf[(y-1)*w+(x+1)]
                     - 2*gray_buf[y*w+(x-1)] + 2*gray_buf[y*w+(x+1)]
                     - gray_buf[(y+1)*w+(x-1)] + gray_buf[(y+1)*w+(x+1)];
            int gy = - gray_buf[(y-1)*w+(x-1)] - 2*gray_buf[(y-1)*w+x] - gray_buf[(y-1)*w+(x+1)]
                     + gray_buf[(y+1)*w+(x-1)] + 2*gray_buf[(y+1)*w+x] + gray_buf[(y+1)*w+(x+1)];
            int mag = ((gx<0?-gx:gx) + (gy<0?-gy:gy));
            mag = (mag > 40) ? mag * 2 : 0;   // threshold out noise
            if (mag > 255) mag = 255;
            rgb[y*w + x] = (mag<<16)|(mag<<8)|mag;
        }
    }
    for (uint32_t x = 0; x < w; x++) { rgb[x] = 0; rgb[(h-1)*w+x] = 0; }
    for (uint32_t y = 0; y < h; y++) { rgb[y*w] = 0; rgb[y*w+(w-1)] = 0; }
}

static void downsample_to_oled(uint32_t *rgb, uint32_t rgb_w, uint32_t rgb_h) {
    for (uint32_t oy = 0; oy < SSD1306_DISPLAY_HEIGHT; oy++){
        for (uint32_t ox = 0; ox < SSD1306_DISPLAY_WIDTH; ox++) {
            uint32_t sx = ox * rgb_w / SSD1306_DISPLAY_WIDTH;
            uint32_t sy = oy * rgb_h / SSD1306_DISPLAY_HEIGHT;
            uint32_t pixel = rgb[sy * rgb_w + sx];
            uint8_t luma = (((pixel>>16)&0xFF) + ((pixel>>8)&0xFF) + (pixel&0xFF)) / 3;
            ssd1306_display_draw_pixel(ox, oy, luma > 80 ? COLOR_WHITE : COLOR_BLACK);
        }
    }
}

// floyd-steinberg dither to the 1-bit OLED: we push each pixel's quantization
// error to its neighbors so shading shows as a pattern instead of a hard threshold.
// pass 1: nearest-neighbor downscale into dither_buf, and convert each pixel to luma via avg(R,G,B).
// pass 2: for each pixel, snap to 0 or 255. then we "diffuse" the error to
//   neighbors on the bottom/right pixels 
// values: (from wikipedia) right 7/16, bottom-left 3/16, below 5/16, bottom-right 1/16
static int16_t dither_buf[SSD1306_DISPLAY_WIDTH * SSD1306_DISPLAY_HEIGHT];
static void dither_to_oled(uint32_t *rgb, uint32_t rgb_w, uint32_t rgb_h) {
    const int W = SSD1306_DISPLAY_WIDTH, H = SSD1306_DISPLAY_HEIGHT;
    for (int oy = 0; oy < H; oy++){
        for (int ox = 0; ox < W; ox++) {
            uint32_t sx = (uint32_t)ox * rgb_w / W;
            uint32_t sy = (uint32_t)oy * rgb_h / H;
            uint32_t p = rgb[sy * rgb_w + sx];
            dither_buf[oy*W + ox] =
                (((p>>16)&0xFF) + ((p>>8)&0xFF) + (p&0xFF)) / 3;
        }
    }

    for (int y = 0; y < H; y++){
        for (int x = 0; x < W; x++) {
            int old = dither_buf[y*W + x];
            int newp = (old >= 128) ? 255 : 0;
            int err = old - newp;
            ssd1306_display_draw_pixel(x, y, newp ? COLOR_WHITE : COLOR_BLACK);
            // spread error: right 7/16, bottom-left 3/16, bottom 5/16, br 1/16
            if (x + 1 < W) dither_buf[y*W + (x+1)] += err * 7 / 16;
            if (y + 1 < H) {
                if (x > 0)     dither_buf[(y+1)*W + (x-1)] += err * 3 / 16;
                dither_buf[(y+1)*W + x] += err * 5 / 16;
                if (x + 1 < W) dither_buf[(y+1)*W + (x+1)] += err * 1 / 16;
            }
        }
    }
}

// centered message on the OLED (size-1 font is ~6px/char)
static void oled_message(const char *msg) {
    ssd1306_display_clear();
    uint32_t len = 0; while (msg[len]) len++;
    uint32_t x = (SSD1306_DISPLAY_WIDTH > len * 6)
               ? (SSD1306_DISPLAY_WIDTH - len * 6) / 2 : 0;
    for (uint32_t i = 0; msg[i]; i++){
        ssd1306_display_draw_character_size(x + i*6, 28, msg[i], COLOR_WHITE, 1, 1);
    }
    ssd1306_display_show();
}

// pixel-art sock
// we used a python script which is currently in the obsolete helpers dir to draw this
// we find that ai/claude is helpful at making lil interactive displays to do silly stuff like this!
#define SOCK_W 12
#define SOCK_H 16
static const uint8_t sock_bmp[SOCK_H][SOCK_W] = {
    {0,0,0,0,0,1,1,1,1,1,1,1},
    {0,0,0,0,0,1,1,1,1,1,1,1},
    {0,0,0,0,0,1,1,1,1,1,1,1},
    {0,0,0,0,0,1,0,0,0,0,0,1},
    {0,0,0,0,0,1,0,0,0,0,0,1},
    {0,0,0,0,0,1,0,0,0,0,0,1},
    {0,0,0,0,0,1,0,0,0,0,0,1},
    {0,0,0,0,0,1,0,0,0,0,0,1},
    {0,0,0,0,1,1,0,0,0,0,0,1},
    {0,0,0,1,1,0,0,0,0,1,1,1},
    {0,1,1,1,0,0,0,0,1,1,0,1},
    {1,1,0,0,0,0,0,1,1,0,0,1},
    {1,0,0,0,0,0,1,1,0,0,1,1},
    {1,0,0,0,0,1,1,0,0,1,1,0},
    {1,1,0,0,0,1,0,0,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,0,0,0},
};

// we like to draw the socks at angles. most cartoon socks online have a cute angle to them!
static void draw_sock_rotated(int ox, int oy, int angle_sign, color_t c) {
    // cos(15)*1000=966, sin(15)*1000=259
    int cx = SOCK_W / 2, cy = SOCK_H / 2;
    for (int y = 0; y < SOCK_H; y++) {
        for (int x = 0; x < SOCK_W; x++) {
            if (!sock_bmp[y][x]) continue;
            int dx = x - cx, dy = y - cy;
            int rx = (dx * 966 - angle_sign * dy * 259) / 1000;
            int ry = (angle_sign * dx * 259 + dy * 966) / 1000;
            int px = ox + cx + rx;
            int py = oy + cy + ry;
            if (px >= 0 && px < SSD1306_DISPLAY_WIDTH &&
                py >= 0 && py < SSD1306_DISPLAY_HEIGHT){
                ssd1306_display_draw_pixel(px, py, c);
            }
        }
    }
}

// title screen with a tilted sock in two corners
// this is purely for graphic display cuteness lol
static void draw_title(void) {
    const char *l1 = "aditi and christy";
    const char *l2 = "capture the world";
    ssd1306_display_clear();
    draw_sock_rotated(1, 1, +1, COLOR_WHITE);   // top-left
    draw_sock_rotated(SSD1306_DISPLAY_WIDTH - SOCK_W - 1,
                      SSD1306_DISPLAY_HEIGHT - SOCK_H - 1,
                      -1, COLOR_WHITE);   // bottom-right
    uint32_t x1 = (SSD1306_DISPLAY_WIDTH - 17 * 6) / 2;  // both lines are 17 chars
    for (uint32_t i = 0; l1[i]; i++){
        ssd1306_display_draw_character_size(x1 + i*6, 22, l1[i], COLOR_WHITE, 1, 1);
    }
    for (uint32_t i = 0; l2[i]; i++){
        ssd1306_display_draw_character_size(x1 + i*6, 36, l2[i], COLOR_WHITE, 1, 1);
    }
    ssd1306_display_show();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// this is fat32 stuff
//////////////////////////////////////////////////////////////////////////////////////////////////////

// OLED view modes, cycled by button A
typedef enum {
    VIEW_TITLE,
    VIEW_COLOR,
    VIEW_EDGE,    // sobel edge detection
    VIEW_DITHER,  // floyd-steinberg dither
    VIEW_COUNT,
} ViewMode;

static const char *view_name(ViewMode m) {
    switch (m) {
    case VIEW_TITLE:  return "TITLE";
    case VIEW_COLOR:  return "COLOR";
    case VIEW_EDGE:   return "EDGE";
    case VIEW_DITHER: return "DITHER";
    default:          return "?";
    }
}

// ---- BT image send logic ----

// send the 20-byte big-endian frame header
static void send_header(uint16_t handle, uint32_t w, uint32_t h,
                        uint32_t fmt, uint32_t payload_len) {
    uint8_t hdr[20];
    uint32_t vals[5] = { FRAME_MAGIC, w, h, fmt, payload_len };
    for (int v = 0; v < 5; v++){
        for (int b = 0; b < 4; b++){
            hdr[v*4 + b] = (vals[v] >> (24 - b*8)) & 0xFF;
        }
    }
    bt_send_packet(handle, hdr, sizeof(hdr));
}

// pack rgb into big-endian RGB565 in send_buf 
// we reused buf as scratch: 2*n bytes, send_buf holds 4*n
// return: the packed byte count.
static uint32_t pack_rgb565(const uint32_t *rgb, uint32_t w, uint32_t h) {
    uint32_t n = w * h;
    uint8_t *buf = (uint8_t *)send_buf;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = rgb[i];
        uint32_t r5 = ((p >> 16) & 0xFF) >> 3;
        uint32_t g6 = ((p >> 8) & 0xFF) >> 2;
        uint32_t b5 = (p & 0xFF) >> 3;
        uint16_t v = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
        buf[i*2] = (v >> 8) & 0xFF;   // big-endian
        buf[i*2 + 1] = v & 0xFF;
    }
    return n * 2;
}

// we send a frame as RGB565 (2 bytes/pixel, which is half the payload of RGB888)
// rgb 565 just packs the msb's of r g and b into 5 6 and 5 bits respectively
// green gets an extra bit bc its spectrum is most recognizable by human eyes
static void send_frame_rgb565(uint16_t handle, const uint32_t *rgb,
                              uint32_t w, uint32_t h) {
    uint32_t nbytes = pack_rgb565(rgb, w, h);
    uint32_t t0 = sys_timer_get_usec();
    send_header(handle, w, h, FMT_RGB565, nbytes);
    int ok = bt_send_raw(handle, (uint8_t *)send_buf, nbytes);
    uint32_t ms = (sys_timer_get_usec() - t0) / 1000;
    if (ok) {
        uint32_t total = nbytes + 20;   // payload + header
        uint32_t kbs = ms ? (total * 1000u / ms) / 1024u : 0;
        printk("[perf] BT send: %d bytes in %d ms = %d KB/s (RGB565 %dx%d)\n",
               (int)total, (int)ms, (int)kbs, (int)w, (int)h);
    } else {
        oled_message("SEND TIMEOUT");
        printk("[aditi_pi] BT send timed out (peer not acking)\n");
        sys_timer_delay_ms(800);
    }
}

// ---- SD card save (FAT32 + writes implemented) ----
// each capture writes IMGnnnnn.QOI to the card root. best-effort: if the card is
// missing/unreadable, saving is disabled at startup and the camera keeps running as before.
static fat32_fs_t g_fs;
static pi_dirent_t g_root;
static int g_sd_ok = 0;
static uint32_t g_save_index = 1;   // IMG00001, IMG00002, ...

// we scan the root for existing IMGnnnnn.*
// reboots keep numbering instead of clobbering IMG00001.
static uint32_t sd_scan_next_index(void) {
    uint32_t maxn = 0;
    unsigned mark = fat_heap_mark();
    pi_directory_t dir = fat32_readdir(&g_fs, &g_root);
    for (uint32_t i = 0; i < dir.ndirents; i++) {
        const char *nm = dir.dirents[i].name;   // 8.3, e.g. "IMG00001.QOI"
        if (nm[0] != 'I' || nm[1] != 'M' || nm[2] != 'G') continue;
        uint32_t v = 0; int ok = 1;
        for (int k = 3; k < 8; k++) {
            if (nm[k] < '0' || nm[k] > '9') { ok = 0; break; }
            v = v * 10 + (uint32_t)(nm[k] - '0');
        }
        if (ok && v > maxn) maxn = v;
    }
    fat_heap_rewind(mark);
    return maxn + 1;
}

// sd card and fat32 bringup code
// we return 1 on success
static int sd_save_init(void) {
    fat_heap_init();
    if (!pi_sd_init_try()) {
        printk("[aditi_pi] SD: no card / init failed -- saving disabled\n");
        return 0;
    }
    mbr_t *mbr = mbr_read();   // panic on a corrupt MBR
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    if (!mbr_part_is_fat32(partition.part_type)) {
        printk("[aditi_pi] SD: partition 0 is not FAT32 -- saving disabled\n");
        return 0;
    }
    g_fs   = fat32_mk(&partition);
    g_root = fat32_get_root(&g_fs);
    g_save_index = sd_scan_next_index();   // cont numbering based on existing IMGs on sd card
    printk("[aditi_pi] SD: FAT32 mounted. n_entries=%d sec/clus=%d nsec/fat=%d "
           "root_clus=%d next_img=%d\n",
           g_fs.n_entries, g_fs.sectors_per_cluster,
           (int)(g_fs.cluster_begin_lba - g_fs.fat_begin_lba),
           g_root.cluster_id, g_save_index);
    return 1;
}

// build "IMGnnnnn.EXT" (8.3, uppercase) into out[13]. ext must be 3 chars.
// notably we use qoi format for our project
static void make_img_name(char *out, uint32_t idx, const char *ext) {
    out[0]='I'; out[1]='M'; out[2]='G';
    for (int i = 7; i >= 3; i--) { out[i] = '0' + (idx % 10); idx /= 10; }
    out[8]='.'; out[9]=ext[0]; out[10]=ext[1]; out[11]=ext[2]; out[12]=0;
}

// create and write one file in the root dir. 
// per-file FAT space is freed after.
static int sd_write_file(const char *name83, const void *data, uint32_t nbytes) {
    if (!g_sd_ok) return 0;
    unsigned mark = fat_heap_mark();
    int ok = 0;
    pi_dirent_t *de = fat32_create(&g_fs, &g_root, (char *)name83, /*is_dir=*/0);
    if (de) {
        pi_file_t f = { .data = (char *)data, .n_alloc = nbytes, .n_data = nbytes };
        ok = fat32_write(&g_fs, &g_root, (char *)name83, &f);
        fat32_flush(&g_fs);
    }
    fat_heap_rewind(mark);
    return ok;
}

// save the current frame to SD as QOI, 
// we downsample to SAVE_WIDTH x SAVE_HEIGHT
// this is 320 by 240 for ours
static void sd_save_capture(const uint32_t *rgb, uint32_t w, uint32_t h) {
    if (!g_sd_ok) return;
    const uint32_t sw = SAVE_WIDTH, sh = SAVE_HEIGHT;
    char name[13];
    unsigned mark = fat_heap_mark();

    // nearest-neighbor -> downsample from full-res -> packed 0x00RRGGBB
    uint32_t *small = fat_kmalloc(sw * sh * sizeof(uint32_t));
    for (uint32_t y = 0; y < sh; y++) {
        uint32_t sy = y * h / sh;
        for (uint32_t x = 0; x < sw; x++){
            small[y*sw + x] = rgb[sy * w + (x * w / sw)];
        }
    }

    uint8_t *qbuf = fat_kmalloc(QOI_MAX_SIZE(sw, sh));
    uint32_t qlen = qoi_encode_rgb(small, sw, sh, qbuf);
    make_img_name(name, g_save_index, "QOI");
    int ok = sd_write_file(name, qbuf, qlen);

    fat_heap_rewind(mark);
    if (ok){
        printk("[aditi_pi] SD: saved IMG%d.QOI %dx%d (%d B)\n",
               (int)g_save_index, (int)sw, (int)sh, (int)qlen);
    } else {
        printk("[aditi_pi] SD: FAILED to save IMG%d.QOI (%d B) -- write error\n",
               (int)g_save_index, (int)qlen);
    }
    g_save_index++;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// FINALLY we get to some real code lol!!
// so far has just been helpers for filtering and sd card/.at32 stuff
//////////////////////////////////////////////////////////////////////////////////////////////////////

// ---- main ----

// notably all the prints are cooked because we used the wrong % formatting L :(

void main() {
    uart_init();
    mmu_enable_caches(); // this must come before BT init since pl011_os uses VBASE virt base addrs
    printk("\n[aditi_pi] main_aditi (camera + OLED + BT) starting\n");

    ssd1306_display_init();
    oled_message("BOOTING");

    // camera init sequence
    if (!camera_init()) { 
        printk("camera init failed\n"); return; 
    }
    if (!camera_set_format(FB_WIDTH, FB_HEIGHT, CAM_FMT_BAYER_10)){
        printk("camera format failed\n"); return;
    }
    camera_set_exposure(10000);
    camera_set_gain(8.0);
    if (!camera_start()) { 
        printk("camera start failed\n"); return; 
    }
    CameraConfig cfg = camera_get_config();
    printk("[aditi_pi] camera %dx%d ready\n", cfg.width, cfg.height);

    // SD card init sequence
    oled_message("SD INIT");
    g_sd_ok = sd_save_init();

    // BT (initiator: wait for christy to accept)
    static const uint8_t peer_addr_be[] = BT_PEER_ADDR_BE;
    oled_message("BT CONNECT");
    bt_setup(BT_LOCAL_NAME, BT_BAUD);
    sys_timer_delay_ms(2000);
    uint16_t bt_handle = bt_connect_initiator(peer_addr_be, BT_PAGE_TIMEOUT);
    printk("[aditi_pi] BT connected, handle=%d\n", bt_handle);

    // buttons init; 
    // they should be set as pullup resistors in button init. (they are connected to gnd/gpio)
    Button btn_a = button_init(BUTTON_PIN_A);   // cycle view mode
    Button btn_b = button_init(BUTTON_PIN_B);   // send frame


    // we're up and running!!
    printk("[aditi_pi] running. A (gpio%d) cycles view, B (gpio%d) sends frame.\n", BUTTON_PIN_A, BUTTON_PIN_B);

    ViewMode mode = VIEW_EDGE;

    // perf stuff: 
    // track OLED render fps per filter mode
    uint32_t perf_t0 = sys_timer_get_usec();
    uint32_t perf_frames = 0;
    ViewMode perf_mode = mode;

    CameraFrame frame;
    // THIS IS OUR MAIN LOOP!!
    while (1) {
        if (!camera_capture_frame(&frame)){
            continue;
        }

        debayer((uint16_t *)frame.buf, rgb_buf,
                cfg.width, cfg.height, cfg.stride, cfg.width);

        // button A: cycle mode
        if (button_was_pressed(&btn_a)) {
            mode = (mode + 1) % VIEW_COUNT;
            printk("[aditi_pi] view mode -> %s\n", view_name(mode));
        }

        // button B: send the full frame (565) to christy/laptop, 
        // then save a downsampled QOI copy to SD.
        if (button_was_pressed(&btn_b)) {
            oled_message("SENDING");
            send_frame_rgb565(bt_handle, rgb_buf, cfg.width, cfg.height);
            if (g_sd_ok) {
                oled_message("SAVING");
                sd_save_capture(rgb_buf, cfg.width, cfg.height);
            }
            continue;
        }

        // main fsm!
        switch (mode) {
        case VIEW_TITLE:
            draw_title();
            break;
        case VIEW_COLOR:
            downsample_to_oled(rgb_buf, cfg.width, cfg.height);
            ssd1306_display_show();
            break;
        case VIEW_EDGE:
            edge_detect(rgb_buf, cfg.width, cfg.height);
            downsample_to_oled(rgb_buf, cfg.width, cfg.height);
            ssd1306_display_show();
            break;
        case VIEW_DITHER:
            dither_to_oled(rgb_buf, cfg.width, cfg.height);
            ssd1306_display_show();
            break;
        default:
            break;
        }

        // perf monitoring
        // we restart perf window on each mode change so we cover each filter 
        // individuallt in reports and dont mix numbers
        if (mode != perf_mode) {
            perf_mode = mode; perf_frames = 0; perf_t0 = sys_timer_get_usec();
        }
        if (++perf_frames >= 30) {
            uint32_t ms = (sys_timer_get_usec() - perf_t0) / 1000;
            uint32_t fps100 = ms ? (perf_frames * 100000u) / ms : 0;  // fps * 100
            printk("[perf] %s: %d.%d%d fps (%d frames in %d ms)\n",
                   view_name(perf_mode), (int)(fps100 / 100),
                   (int)((fps100 % 100) / 10), (int)(fps100 % 10),
                   (int)perf_frames, (int)ms);
            perf_frames = 0; perf_t0 = sys_timer_get_usec();
        }
    }

    camera_stop();
}
