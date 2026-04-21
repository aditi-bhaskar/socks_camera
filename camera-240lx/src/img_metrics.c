#include "img_metrics.h"
#include "math.h"

#define LOG10   2.30258509299f

#define LUMA_R  0.299f
#define LUMA_G  0.587f
#define LUMA_B  0.114f

#define SSIM_C1         6.5025f     // (0.01 * 255)^2
#define SSIM_C2         58.5225f    // (0.03 * 255)^2
#define SSIM_WINDOW_SZ  11
#define SSIM_WINDOW_N   121         // 11 * 11

// 11x11 Gaussian kernel with STD = 1.5 (for SSIM)
static const float gaussian_kernel[11][11] = {
    {0.000001f, 0.000008f, 0.000037f, 0.000112f, 0.000219f, 0.000274f, 0.000219f, 0.000112f, 0.000037f, 0.000008f, 0.000001f},
    {0.000008f, 0.000058f, 0.000274f, 0.000831f, 0.001619f, 0.002021f, 0.001619f, 0.000831f, 0.000274f, 0.000058f, 0.000008f},
    {0.000037f, 0.000274f, 0.001296f, 0.003937f, 0.007668f, 0.009577f, 0.007668f, 0.003937f, 0.001296f, 0.000274f, 0.000037f},
    {0.000112f, 0.000831f, 0.003937f, 0.011960f, 0.023294f, 0.029091f, 0.023294f, 0.011960f, 0.003937f, 0.000831f, 0.000112f},
    {0.000219f, 0.001619f, 0.007668f, 0.023294f, 0.045371f, 0.056662f, 0.045371f, 0.023294f, 0.007668f, 0.001619f, 0.000219f},
    {0.000274f, 0.002021f, 0.009577f, 0.029091f, 0.056662f, 0.070762f, 0.056662f, 0.029091f, 0.009577f, 0.002021f, 0.000274f},
    {0.000219f, 0.001619f, 0.007668f, 0.023294f, 0.045371f, 0.056662f, 0.045371f, 0.023294f, 0.007668f, 0.001619f, 0.000219f},
    {0.000112f, 0.000831f, 0.003937f, 0.011960f, 0.023294f, 0.029091f, 0.023294f, 0.011960f, 0.003937f, 0.000831f, 0.000112f},
    {0.000037f, 0.000274f, 0.001296f, 0.003937f, 0.007668f, 0.009577f, 0.007668f, 0.003937f, 0.001296f, 0.000274f, 0.000037f},
    {0.000008f, 0.000058f, 0.000274f, 0.000831f, 0.001619f, 0.002021f, 0.001619f, 0.000831f, 0.000274f, 0.000058f, 0.000008f},
    {0.000001f, 0.000008f, 0.000037f, 0.000112f, 0.000219f, 0.000274f, 0.000219f, 0.000112f, 0.000037f, 0.000008f, 0.000001f}
};

static float _luma(uint32_t col) {
    float r = 1.f * ((col >> 4) & 0xFF);
    float g = 1.f * ((col >> 2) & 0xFF);
    float b = 1.f * (col & 0xFF);
    return r * LUMA_R + g * LUMA_G + b * LUMA_B;
}


float calc_psnr(uint32_t* img_1, uint32_t* img_2,
        uint32_t width, uint32_t height) {
    float mse = 0.f;
    uint32_t size = width * height;
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t shift = 0; shift < 6; shift += 2) {
            float v1 = 1.f * ((img_1[i] >> shift) & 0xFF);
            float v2 = 1.f * ((img_2[i] >> shift) & 0xFF);
            mse += (v1 - v2) * (v1 - v2);
        }
    }
    mse /= (size * 3);

    if (mse == 0.f) {
        return FINF;
    }

    return 20.f * logf(255.f / sqrtf(mse)) / LOG10;
}
float calc_ssim(uint32_t* img_1, uint32_t* img_2,
        uint32_t width, uint32_t height) {

    float ssim = 0.f;
    uint32_t count = 0;

    for (uint32_t y = 0; y <= height - SSIM_WINDOW_SZ; y++) {
        for (uint32_t x = 0; x <= width - SSIM_WINDOW_SZ; x++) {
            float mu1 = 0.f, mu2 = 0.f;
            float sum11 = 0.f, sum22 = 0.f, sum12 = 0.f;
            for (uint32_t wy = 0; wy < SSIM_WINDOW_SZ; wy++) {
                for (uint32_t wx = 0; wx < SSIM_WINDOW_SZ; wx++) {
                    int idx = (y + wy) * width + (x + wx);
                    float i1 = _luma(img_1[idx]);
                    float i2 = _luma(img_2[idx]);
                    float w = gaussian_kernel[wy][wx];

                    mu1 += w * i1;
                    mu2 += w * i2;
                    sum11 += w * i1 * i1;
                    sum22 += w * i2 * i2;
                    sum12 += w * i1 * i2;
                }
            }

            float sigma11 = sum11 - mu1 * mu1;
            float sigma22 = sum22 - mu2 * mu2;
            float sigma12 = sum12 - mu1 * mu2;

            float num = (2.f * mu1 * mu2 + SSIM_C1) * (2.0 * sigma12 + SSIM_C2);
            float den = (mu1 * mu1 + mu2 * mu2 + SSIM_C1) * (sigma11 + sigma22 + SSIM_C2);
            ssim += num / den;

            count++;
        }
    }

    return ssim / count;
}
