#ifndef QOI_H
#define QOI_H

#include <stdint.h>

// Worst-case encoded size for a w*h RGB image: one 4-byte QOI_OP_RGB per pixel,
// plus the 14-byte header and the 8-byte end marker.
#define QOI_MAX_SIZE(w, h) ((w) * (h) * 4u + 14u + 8u)

// Encode a packed-RGB framebuffer (0x00RRGGBB per pixel, row-major, w*h pixels)
// as a 3-channel QOI image into `out`. `out` must be at least QOI_MAX_SIZE(w,h)
// bytes. Returns the number of bytes written. Lossless, single pass, no
// floating point -- fast enough for a Pi Zero.
uint32_t qoi_encode_rgb(const uint32_t *rgb, uint32_t w, uint32_t h, uint8_t *out);

#endif // QOI_H
