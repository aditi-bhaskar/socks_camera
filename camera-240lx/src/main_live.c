// this, we used some amount of AI to write
// we came up with the tiling ideas though to help with the streamming

// main_live.c — livestream-only test (no OLED, no buttons, no SD).
//
// connects to christy and continuously streams a small feed over BT. to keep it
// fast we only send the TILES that changed since the last frame, with a periodic
// full keyframe to stop drift. each message is still wrapped in the normal
// 20-byte capture header (MAGIC | W | H | FMT | PAYLOAD_LEN), so the existing
// frame-at-a-time christy forwards it unchanged -- it just sees a stream of
// small "frames". view it with laptop-side-live.py.
//
//   make BUILD=live


#include "lib.h"
#include "uart.h"
#include "camera.h"
#include "mmu.h"
#include "sys_timer.h"
#include "bt.h"

#define FB_WIDTH  640
#define FB_HEIGHT 480

// streamed resolution + tiling. must divide FB_WIDTH/FB_HEIGHT and TILE.
#define LIVE_W   160
#define LIVE_H   120
#define TILE     8
#define TILES_X  (LIVE_W / TILE)          // 20
#define TILES_Y  (LIVE_H / TILE)          // 15

#define KEYFRAME_INTERVAL 30   // force a full keyframe every N frames
#define LUMA_THRESH       12   // per-pixel luma delta that counts as "changed"
#define TILE_MIN_CHANGED  6    // changed pixels (of 64) needed to resend a tile

#define BT_LOCAL_NAME   "RPI Aditi"
#define BT_PEER_ADDR_BE { 0xb8, 0x27, 0xeb, 0x30, 0xd1, 0x77 }   // christy
#define BT_BAUD         3000000
#define BT_PAGE_TIMEOUT 0xFFFF

#define FRAME_MAGIC      0x42424242
#define FMT_LIVE_KEY     2   // payload = LIVE_W*LIVE_H*2 big-endian RGB565
#define FMT_LIVE_DELTA   3   // payload = ntiles(2) + ntiles*(idx(2)+TILE*TILE*2)

static uint32_t rgb_buf[FB_WIDTH * FB_HEIGHT];   // debayered frame
static uint16_t cur[LIVE_W * LIVE_H];            // current frame, host-endian 565
static uint16_t ref[LIVE_W * LIVE_H];            // last SENT frame, for diffing
static uint8_t  pbuf[2 + TILES_X * TILES_Y * (2 + TILE * TILE * 2)];  // payload

// same debayer + white balance as main.c / main_aditi.
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
            r = (r * 555) >> 8; g = (g * 384) >> 8; b = (b * 571) >> 8;  // WB gains
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            rgb[y * out_stride_pixels + x] = (r << 16) | (g << 8) | b;
        }
    }
}

// nearest-neighbor downsample rgb (w x h) -> cur[] as host-endian RGB565.
static void downsample_565(const uint32_t *rgb, uint32_t w, uint32_t h) {
    for (uint32_t oy = 0; oy < LIVE_H; oy++) {
        uint32_t sy = oy * h / LIVE_H;
        for (uint32_t ox = 0; ox < LIVE_W; ox++) {
            uint32_t p = rgb[sy * w + (ox * w / LIVE_W)];
            uint32_t r5 = ((p >> 16) & 0xFF) >> 3;
            uint32_t g6 = ((p >>  8) & 0xFF) >> 2;
            uint32_t b5 = ( p        & 0xFF) >> 3;
            cur[oy * LIVE_W + ox] = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
        }
    }
}

// rough luma (0..~249) from a 565 value, for change detection.
static int luma565(uint16_t v) {
    int r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
    return ((r << 3) + (g << 2) + (b << 3)) / 3;
}

// 20-byte big-endian header: MAGIC | W | H | FMT | PAYLOAD_LEN.
static void send_header(uint16_t handle, uint32_t fmt, uint32_t len) {
    uint8_t hdr[20];
    uint32_t vals[5] = { FRAME_MAGIC, LIVE_W, LIVE_H, fmt, len };
    for (int v = 0; v < 5; v++)
        for (int b = 0; b < 4; b++)
            hdr[v*4 + b] = (vals[v] >> (24 - b*8)) & 0xFF;
    bt_send_packet(handle, hdr, sizeof(hdr));
}

// build + send a full keyframe (all of cur[]), and reset ref[] to match.
static void send_keyframe(uint16_t handle) {
    uint32_t off = 0;
    for (uint32_t i = 0; i < LIVE_W * LIVE_H; i++) {
        uint16_t v = cur[i];
        pbuf[off++] = v >> 8; pbuf[off++] = v & 0xFF;   // big-endian
        ref[i] = v;
    }
    send_header(handle, FMT_LIVE_KEY, off);
    bt_send_raw(handle, pbuf, off);
}

// build + send only the changed tiles. returns the number of tiles sent.
static int send_delta(uint16_t handle) {
    uint32_t off = 2;            // reserve 2 bytes for ntiles
    uint16_t ntiles = 0;

    for (int ty = 0; ty < TILES_Y; ty++)
        for (int tx = 0; tx < TILES_X; tx++) {
            int changed = 0;
            for (int dy = 0; dy < TILE; dy++)
                for (int dx = 0; dx < TILE; dx++) {
                    uint32_t idx = (ty*TILE + dy) * LIVE_W + (tx*TILE + dx);
                    int d = luma565(cur[idx]) - luma565(ref[idx]);
                    if (d < 0) d = -d;
                    if (d > LUMA_THRESH) changed++;
                }
            if (changed < TILE_MIN_CHANGED) continue;

            uint16_t tindex = ty * TILES_X + tx;
            pbuf[off++] = tindex >> 8; pbuf[off++] = tindex & 0xFF;
            for (int dy = 0; dy < TILE; dy++)
                for (int dx = 0; dx < TILE; dx++) {
                    uint32_t idx = (ty*TILE + dy) * LIVE_W + (tx*TILE + dx);
                    uint16_t v = cur[idx];
                    pbuf[off++] = v >> 8; pbuf[off++] = v & 0xFF;
                    ref[idx] = v;   // sync ref for the tile we're sending
                }
            ntiles++;
        }

    if (ntiles == 0) return 0;   // nothing changed, send nothing
    pbuf[0] = ntiles >> 8; pbuf[1] = ntiles & 0xFF;
    send_header(handle, FMT_LIVE_DELTA, off);
    bt_send_raw(handle, pbuf, off);
    return ntiles;
}

void main() {
    uart_init();
    mmu_enable_caches();   // MUST be before BT init: pl011_os uses VBASE addrs
    printk("\n[live] main_live (tile-delta livestream test) starting\n");

    if (!camera_init())            { printk("camera init failed\n"); return; }
    if (!camera_set_format(FB_WIDTH, FB_HEIGHT, CAM_FMT_BAYER_10))
                                   { printk("camera format failed\n"); return; }
    camera_set_exposure(10000);
    camera_set_gain(8.0);
    if (!camera_start())           { printk("camera start failed\n"); return; }
    CameraConfig cfg = camera_get_config();
    printk("[live] camera %dx%d ready, streaming %dx%d (tile %d)\n",
           cfg.width, cfg.height, LIVE_W, LIVE_H, TILE);

    static const uint8_t peer_addr_be[] = BT_PEER_ADDR_BE;
    printk("[live] bt_setup + dialing christy...\n");
    bt_setup(BT_LOCAL_NAME, BT_BAUD);
    sys_timer_delay_ms(2000);
    uint16_t handle = bt_connect_initiator(peer_addr_be, BT_PAGE_TIMEOUT);
    printk("[live] BT connected, handle=%d. streaming...\n", handle);

    uint32_t t0 = sys_timer_get_usec(), frames = 0, count = 0;

    CameraFrame frame;
    while (1) {
        if (!camera_capture_frame(&frame))
            continue;
        debayer((uint16_t *)frame.buf, rgb_buf,
                cfg.width, cfg.height, cfg.stride, cfg.width);
        downsample_565(rgb_buf, cfg.width, cfg.height);

        if (count % KEYFRAME_INTERVAL == 0) send_keyframe(handle);
        else                                send_delta(handle);
        count++;

        if (++frames >= 20) {
            uint32_t ms = (sys_timer_get_usec() - t0) / 1000;
            uint32_t fps100 = ms ? (frames * 100000u) / ms : 0;
            printk("[live] %d.%d%d fps (%d frames in %d ms, %dx%d)\n",
                   (int)(fps100 / 100), (int)((fps100 % 100) / 10),
                   (int)(fps100 % 10), (int)frames, (int)ms, LIVE_W, LIVE_H);
            frames = 0; t0 = sys_timer_get_usec();
        }
    }

    camera_stop();
}
