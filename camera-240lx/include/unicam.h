#ifndef UNICAM_H
#define UNICAM_H

#include "vm.h"
#include "camera_buffer.h"

#include <stdint.h>
#include <stdbool.h>

#define UNICAM_BASE     (0x20801000 | KERNEL_VBASE)  // CSI1 for camera port
#define CAM_CLK_BASE    (0x20101000 | KERNEL_VBASE)  // Clock manager for CSI
#define MBOX_TAG_UNICAM 0x38030

#define UNICAM_CTRL     0x000
#define UNICAM_STA      0x004
#define UNICAM_ANA      0x008
#define UNICAM_PRI      0x00C
#define UNICAM_CLK      0x010
#define UNICAM_CLT      0x014
#define UNICAM_DAT0     0x018
#define UNICAM_DAT1     0x01C
#define UNICAM_DLT      0x028
#define UNICAM_CMP0     0x02C
#define UNICAM_ICTL     0x100
#define UNICAM_ISTA     0x104
#define UNICAM_IDI0     0x108
#define UNICAM_IPIPE    0x10C
#define UNICAM_IBSA0    0x110
#define UNICAM_IBEA0    0x114
#define UNICAM_IBLS     0x118
#define UNICAM_IHWIN    0x120
#define UNICAM_IVWIN    0x128
#define UNICAM_DCS      0x200
#define UNICAM_MISC     0x400

// CTRL register bits
#define UNICAM_CPE      (1 << 0)   // Capture enable
#define UNICAM_MEM      (1 << 1)   // Memory input mode
#define UNICAM_CPR      (1 << 2)   // Capture peripheral reset
#define UNICAM_SOE      (1 << 4)   // Stop output engine

// STA register bits
#define UNICAM_SYN      (1 << 0)
#define UNICAM_CS       (1 << 1)
#define UNICAM_PS       (1 << 13)
#define UNICAM_IS       (1 << 14)
#define UNICAM_PI0      (1 << 15)
#define UNICAM_FSI_S    (1 << 17)
#define UNICAM_FEI_S    (1 << 18)

// ANA register bits
#define UNICAM_AR       (1 << 2)   // Analog reset
#define UNICAM_DDL      (1 << 3)   // Data lane disable

// ICTL register bits
#define UNICAM_FSIE     (1 << 0)   // Frame start interrupt enable
#define UNICAM_FEIE     (1 << 1)   // Frame end interrupt enable
#define UNICAM_IBOB     (1 << 2)   // Image buffer on burst

// ISTA register bits
#define UNICAM_FSI      (1 << 0)   // Frame start
#define UNICAM_FEI      (1 << 1)   // Frame end

// CLK register bits
#define UNICAM_CLE      (1 << 0)   // Clock lane enable
#define UNICAM_CLLPE    (1 << 2)   // Clock lane LP enable

// DAT register bits
#define UNICAM_DLE      (1 << 0)   // Data lane enable
#define UNICAM_DLLPE    (1 << 2)   // Data lane LP enable

// IPIPE register - unpacking/packing modes
#define UNICAM_PUM_NONE     0
#define UNICAM_PUM_UNPACK10 4
#define UNICAM_PPM_NONE     0
#define UNICAM_PPM_PACK16   5

// CMP0 register bits
#define UNICAM_PCE      (1 << 31)
#define UNICAM_GI       (1 << 9)
#define UNICAM_CPH      (1 << 8)

// MISC register bits
#define UNICAM_FL0      (1 << 6)
#define UNICAM_FL1      (1 << 9)

// CSI-2 data type codes
#define CSI2_DT_RAW8    0x2A
#define CSI2_DT_RAW10   0x2B

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t depth;
    uint32_t buffer_size;
} UnicamConfig;

bool unicam_init();
void unicam_deinit();
bool unicam_configure(UnicamConfig* cfg);
bool unicam_start();
void unicam_stop();
bool unicam_wait_frame();

#endif
