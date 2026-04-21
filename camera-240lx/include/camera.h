#ifndef CAMERA_H
#define CAMERA_H

#include "camera_buffer.h"


typedef enum {
    CAM_FMT_BAYER_8,
    CAM_FMT_BAYER_10, // 16-bit pixels
} CameraFormat;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t stride; // bytes per row
    CameraFormat format;
} CameraConfig;

typedef struct {
    uint32_t exposure;  // microseconds
    float gain;         // total gain multiplier
    float ana_gain;     // analog gain
    float dig_gain;     // digital gain
    uint32_t white_bal;
    uint32_t ts;        // timestamp
    uint32_t error;     // difference after intended timestamp
    uint32_t delay;     // delay from last frame (if in sequence)
} CameraShot;

typedef struct {
    CameraBuffer* buf;
    uint32_t size;
    uint32_t sequence;

    CameraConfig cfg;
    CameraShot shot;
} CameraFrame;

bool camera_init();
bool camera_set_format(uint32_t width, uint32_t height, CameraFormat fmt);
CameraConfig camera_get_config();

bool camera_start();
void camera_stop();

bool camera_capture_frame(CameraFrame* frame);
bool camera_capture_frame_shot(CameraFrame* frame, CameraShot shot);
uint32_t camera_capture_frames(CameraFrame* frames, CameraShot* shots, uint32_t num_shots);

bool camera_set_exposure(uint32_t us);
bool camera_set_gain(float gain);
bool camera_set_analog_gain(float ana_gain);
bool camera_set_digital_gain(float dig_gain);
bool camera_set_vflip(bool enable);
bool camera_set_hflip(bool enable);

#endif
