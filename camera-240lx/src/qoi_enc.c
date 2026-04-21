// Minimal QOI (Quite OK Image) encoder for 3-channel RGB.
// Format reference: https://qoiformat.org/qoi-specification.pdf
// Single pass, integer-only, lossless.
// we fix alpha at 255 (opaque), so the RGBA op is never created.

#include "qoi.h"

#define QOI_OP_INDEX 0x00 // 00xxxxxx
#define QOI_OP_DIFF  0x40 // 01xxxxxx
#define QOI_OP_LUMA  0x80 // 10xxxxxx
#define QOI_OP_RUN   0xC0 // 11xxxxxx
#define QOI_OP_RGB   0xFE // 11111110
#define QOI_OP_RGBA  0xFF // 11111111

#define QOI_HASH(r, g, b, a) (((r)*3 + (g)*5 + (b)*7 + (a)*11) & 63)

static void put_u32_be(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF; p[1] = (v >> 16) & 0xFF;
    p[2] = (v >>  8) & 0xFF; p[3] =  v        & 0xFF;
}

// see wikipedia for more info
// this reminded me of like leetcode compression algos
uint32_t qoi_encode_rgb(const uint32_t *rgb, uint32_t w, uint32_t h, uint8_t *out) {
    uint32_t p = 0;

    // ---- 14-byte header ----
    out[p++] = 'q'; out[p++] = 'o'; out[p++] = 'i'; out[p++] = 'f';
    put_u32_be(out + p, w); p += 4;
    put_u32_be(out + p, h); p += 4;
    out[p++] = 3; // channels: RGB
    out[p++] = 0; // colorspace: sRGB with linear alpha

    // index of the 64 most-recently-seen pixels (packed r,g,b,a), zeroed.
    uint8_t idx_r[64] = {0}, idx_g[64] = {0}, idx_b[64] = {0}, idx_a[64] = {0};

    // previous pixel starts at {0,0,0,255} per the spec.
    uint8_t pr = 0, pg = 0, pb = 0;
    const uint8_t pa = 255; // alpha constant for our RGB frames

    uint32_t run = 0;
    uint32_t npx = w * h;

    for (uint32_t i = 0; i < npx; i++) {
        uint32_t px = rgb[i];
        uint8_t r = (px >> 16) & 0xFF;
        uint8_t g = (px >>  8) & 0xFF;
        uint8_t b =  px        & 0xFF;

        if (r == pr && g == pg && b == pb) {
            run++;
            if (run == 62 || i == npx - 1) {
                out[p++] = QOI_OP_RUN | (run - 1);
                run = 0;
            }
        } else {
            if (run > 0) {
                out[p++] = QOI_OP_RUN | (run - 1);
                run = 0;
            }

            int h6 = QOI_HASH(r, g, b, pa);
            if (idx_r[h6] == r && idx_g[h6] == g && idx_b[h6] == b && idx_a[h6] == pa) {
                out[p++] = QOI_OP_INDEX | h6;
            } else {
                idx_r[h6] = r; idx_g[h6] = g; idx_b[h6] = b; idx_a[h6] = pa;

                int vr = (int)r - (int)pr;
                int vg = (int)g - (int)pg;
                int vb = (int)b - (int)pb;
                int vg_r = vr - vg;
                int vg_b = vb - vg;

                if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
                    out[p++] = QOI_OP_DIFF | ((vr + 2) << 4) | ((vg + 2) << 2) | (vb + 2);
                } else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 && vg_b > -9 && vg_b < 8) {
                    out[p++] = QOI_OP_LUMA  | (vg + 32);
                    out[p++] = ((vg_r + 8) << 4) | (vg_b + 8);
                } else {
                    out[p++] = QOI_OP_RGB;
                    out[p++] = r; out[p++] = g; out[p++] = b;
                }
            }
        }
        pr = r; pg = g; pb = b;
    }

    // ---- 8-byte end marker: seven 0x00 then 0x01 ----
    for (int k = 0; k < 7; k++) out[p++] = 0x00;
    out[p++] = 0x01;
    return p;
}
