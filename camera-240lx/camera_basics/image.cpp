#include "image.h"

#include <algorithm>
#include <fstream>
#include <cassert>

Image::Image(int width, int height, int depth, std::vector<float> data) :
    width_(width),
    height_(height),
    size_(width * height),
    depth_(depth),
    data_(std::move(data)) {

    if (data_.size() == (size_t) size_) {
        fmt_ = GRAY;
    } else {
        assert(data_.size() == (size_t) size_ * 3);
        fmt_ = RGB;
    }
}

// RGGB bayer
int Image::get_bayer_channel(int i) const {
    assert(fmt_ == GRAY);

    if (i < 0 || i >= size_) return -1;
    int y = i / width_, x = i % width_;
    return (y & 1) * 2 + (x & 1);
}

void Image::black_white_norm(int white_level, int black_level) {
    for (float &i : data_) {
        i = std::clamp(i - black_level, 0.f, (float) white_level - black_level);
        i *= 1.f * pixel_max(depth_) / (white_level - black_level);
    }
}

void Image::gray_world_wb(bool wb_intensity,
        float wb_intensity_threshold) {
    std::vector<float> avg(4);
    std::vector<int> cnt(4);
    for (int i = 0; i < size_; i++) {
        if (!wb_intensity || data_[i] < pixel_max(depth_) * wb_intensity_threshold) {
            int c = get_bayer_channel(i);
            avg[c] += data_[i];
            cnt[c]++;
        }
    }

    for (int i = 0; i < 4; i++) {
        avg[i] /= cnt[i];
    }

    double g_avg = (avg[1] + avg[2]) / 2;
    for (int i = 0; i < size_; i++) {
        int c = get_bayer_channel(i);
        double gain = g_avg / avg[c];
        data_[i] = (int) std::clamp(gain * data_[i], 0.0, (double) pixel_max(depth_));
    }
}

void Image::debayer() {
    assert(fmt_ == GRAY);
    std::vector<float> new_data(size_ * 3);

    // assumes RGGB
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            if (y == 0 || x == 0) {
                continue;
            }

            std::vector<float>& p = data_;
            int i = y * width_ + x;
            int s = width_;

            int r, g, b;
            if (y & 1) {
                if (x & 1) { // b
                    r = (p[i - s - 1] + p[i - s + 1] + p[i + s - 1] + p[i + s + 1]) / 4;
                    g = (p[i - 1] + p[i + 1] + p[i - s] + p[i + s]) / 4;
                    b = p[i];
                } else { // g2
                    r = (p[i - s] + p[i + s]) / 2;
                    g = p[i];
                    b = (p[i - 1] + p[i + 1]) / 2;
                }
            } else {
                if (x & 1) { // g1
                    r = (p[i - 1] + p[i + 1]) / 2;
                    g = p[i];
                    b = (p[i - s] + p[i + s]) / 2;
                } else { // r
                    r = p[i];
                    g = (p[i - 1] + p[i + 1] + p[i - s] + p[i + s]) / 4;
                    b = (p[i - s - 1] + p[i - s + 1] + p[i + s - 1] + p[i + s + 1]) / 4;
                }
            }

            new_data[i * 3] = r;
            new_data[i * 3 + 1] = g;
            new_data[i * 3 + 2] = b;
        }
    }

    data_.swap(new_data);
    fmt_ = RGB;
}

void Image::write_ppm(std::string filename) {
    std::ofstream fout(filename + ".ppm");
    if (fmt_ == GRAY) {
        fout << "P2\n" << width_ << " " << height_ << '\n';
    } else {
        fout << "P3\n" << width_ << " " << height_ << '\n';
    }

    fout << pixel_max(depth_) << '\n';
    for (float f : data_) {
        fout << static_cast<int>(f) << " ";
    }

    fout.close();
}

std::vector<Image::Pixel> Image::get_pixels() {
    std::vector<Image::Pixel> pixels(size_);
    if (fmt_ == GRAY) {
        for (int i = 0; i < size_; i++) {
            pixels[i] = Image::Pixel {
                static_cast<int>(data_[i]),
                static_cast<int>(data_[i]),
                static_cast<int>(data_[i])
            };
        }
    } else {
        for (int i = 0; i < size_; i++) {
            pixels[i] = Image::Pixel {
                static_cast<int>(data_[i * 3]),
                static_cast<int>(data_[i * 3 + 1]),
                static_cast<int>(data_[i * 3 + 2])
            };
        }
    }
    return pixels;
}

