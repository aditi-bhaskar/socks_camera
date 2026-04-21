#include "image.h"

#include <fstream>

const std::string input_fname = "input.txt";
const std::string output_prefix = "output_";
const int num_imgs = 5;
const int width = 640;
const int height = 480;
const int img_size = width * height;

// black/white levels on 10-bit pixels
const int black_level = 64;
const int white_level = 1023;

const double wb_intensity_threshold = 0.95;
const bool wb_intensity = false;

int main() {
    std::ifstream fin(input_fname);

    for (int cur_img = 0; cur_img < num_imgs; cur_img++) {
        std::vector<float> bayer(img_size);
        for (float &i : bayer) {
            fin >> i;
        }

        Image img(width, height, 10, bayer);
        img.write_ppm(output_prefix + "bayer_" + std::to_string(cur_img));
        img.black_white_norm(white_level, black_level);
        img.write_ppm(output_prefix + "norm_" + std::to_string(cur_img));
        img.gray_world_wb(wb_intensity, wb_intensity_threshold);
        img.write_ppm(output_prefix + "wb_" + std::to_string(cur_img));
        img.debayer();
        img.write_ppm(output_prefix + std::to_string(cur_img));
    }

    return 0;
}
