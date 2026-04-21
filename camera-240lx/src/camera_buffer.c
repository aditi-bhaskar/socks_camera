#include "camera_buffer.h"
#include "lib.h"
#include "mmu.h"

#include <stddef.h>

// one extra buffer to store currently processing
static CameraBuffer g_bufs[CAM_MAX_BUFFERS + 1];
static CameraBuffer* g_buf_ptrs[CAM_MAX_BUFFERS + 1];
static uint8_t g_num_buffers;
static int g_ready_idx;
static int g_write_idx;

bool camera_buffer_init(uint32_t num_buffers) {
    if (num_buffers > CAM_MAX_BUFFERS) {
        printk("camera buffer: too many buffers\n");
        return false;
    }
    if (num_buffers < CAM_MIN_BUFFERS) {
        printk("camera buffer: not enough buffers\n");
        return false;
    }

    g_ready_idx = -1;
    g_write_idx = 0;
    g_num_buffers = num_buffers;
    for (uint32_t i = 0; i <= num_buffers; i++) {
        g_buf_ptrs[i] = &g_bufs[i];
    }

    printk("camera buffer: initialized\n");
    return true;
}

CameraBuffer* camera_buffer_advance(bool set_ready) {
    if (set_ready) g_ready_idx = g_write_idx;
    g_write_idx = (g_write_idx + 1) % g_num_buffers;
    return g_buf_ptrs[g_write_idx];
}
CameraBuffer* camera_buffer_get_write() {
    return g_buf_ptrs[g_write_idx];
}
CameraBuffer* camera_buffer_get_ready() {
    if (g_ready_idx == -1) return NULL;

    // disable interrupts
    asm volatile ("cpsid i");

    // swap ready idx with processing idx
    CameraBuffer* res = g_buf_ptrs[g_ready_idx];
    g_buf_ptrs[g_ready_idx] = g_buf_ptrs[g_num_buffers];
    g_buf_ptrs[g_num_buffers] = res;
    mmu_flush_dcache();

    // enable interrupts
    asm volatile ("cpsie i");

    return res;
}
bool camera_buffer_save_ready(CameraBuffer* buf, uint32_t size) {
    if (g_ready_idx == -1) return false;

    // copy ready buffer into given buffer (TODO: optimize with DMA?)
    asm volatile ("cpsid i");
    memcpy(buf, g_buf_ptrs[g_ready_idx], size);
    mmu_flush_dcache();
    asm volatile ("cpsie i");

    return true;
}

