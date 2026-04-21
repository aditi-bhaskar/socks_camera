#pragma once

#include <vector>
#include <string>

class Image {
public:
    enum PixelFormat { GRAY = 1, RGB = 3 };
    struct Pixel { int r = 0, g = 0, b = 0; };

    Image(int width, int height, int depth, std::vector<float> data);

    void black_white_norm(int white_level, int black_level);
    void gray_world_wb(bool wb_intensity, float wb_intensity_threshold);
    void debayer();
    void write_ppm(std::string filename);

    std::vector<Pixel> get_pixels();

private:
    int width_;
    int height_;
    int size_;
    PixelFormat fmt_;
    int depth_;
    std::vector<float> data_;

    int get_bayer_channel(int i) const;
    static int pixel_max(int depth) { return (1 << depth) - 1; }
};

