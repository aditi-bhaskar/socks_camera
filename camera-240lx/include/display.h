#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifndef DISPLAY_CONFIG_T
#define DISPLAY_CONFIG_T
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t* buffer; // current buffer (virt addr)
} DisplayConfig;
#endif

bool display_init(uint32_t width, uint32_t height);
DisplayConfig display_get_config();
uint32_t* display_get_buffer();
void display_swap();

#endif
