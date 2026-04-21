#ifndef CAMERA_BUFFER_H
#define CAMERA_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

// 1920 x 1080 x sizeof(uint16_t)
#define CAM_MAX_FRAME_SIZE  (1920 * 1080 * 2)
#define CAM_MIN_BUFFERS     3
#define CAM_MAX_BUFFERS     8

typedef struct {
    __attribute__((aligned(32)))
    uint8_t buf[CAM_MAX_FRAME_SIZE];
} CameraBuffer;

bool camera_buffer_init(uint32_t num_buffers);

CameraBuffer* camera_buffer_advance(bool set_ready);
CameraBuffer* camera_buffer_get_write();
CameraBuffer* camera_buffer_get_ready();
bool camera_buffer_save_ready(CameraBuffer* buf, uint32_t size);

#endif
