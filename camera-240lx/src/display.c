#include "display.h"
#include "lib.h"
#include "mailbox_interface.h"
#include "vm.h"
#include "mmu.h"

// TODO: move this to rpi_os?
#define MBOX_TAG_WAIT_FOR_VSYNC 0x0004800E

static uint32_t g_width;
static uint32_t g_height;
static uint32_t g_pitch;
static uint32_t* g_fb_base;
static int g_current_offset;

#define NEXT_OFFSET ((g_current_offset == 0) ? g_height : 0)

static void wait_vsync() {
    assert(mbox_get_property_batch(4,
        MBOX_TAG_WAIT_FOR_VSYNC, 4, 0, 0
    ), "Display wait for VSYNC failed");
}

bool display_init(uint32_t width, uint32_t height) {
    g_width = width;
    g_height = height;
    
    uint32_t* fb_phys = 0;
    mbox_framebuffer_init(width, height, width, height * 2, 32, &fb_phys);
    if (!fb_phys) {
        return false;
    }
    
    g_pitch = mbox_framebuffer_get_pitch();
    g_fb_base = (uint32_t*) __va(fb_phys);
    g_current_offset = 0;
    
    printk("display: %dx%d pitch=%d\n", width, height, g_pitch);
    return true;
}

DisplayConfig display_get_config() {
    DisplayConfig cfg = {
        .width = g_width,
        .height = g_height,
        .pitch = g_pitch,
        .buffer = display_get_buffer()
    };
    return cfg;
}

uint32_t* display_get_buffer() {
    return g_fb_base + NEXT_OFFSET * (g_pitch / 4);
}

void display_swap() {
    mem_barrier_dsb();
    
    uint32_t nxt_offset = NEXT_OFFSET;
    wait_vsync();
    mbox_framebuffer_set_virtual_offset(0, nxt_offset);
    g_current_offset = nxt_offset;
    mmu_flush_dcache();
}
