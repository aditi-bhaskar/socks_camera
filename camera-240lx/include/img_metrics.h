#ifndef IMG_METRICS_H
#define IMG_METRICS_H

#include <stdint.h>

float calc_psnr(uint32_t* img_1, uint32_t* img_2,
        uint32_t width, uint32_t height);
float calc_ssim(uint32_t* img_1, uint32_t* img_2,
        uint32_t width, uint32_t height);

#endif
