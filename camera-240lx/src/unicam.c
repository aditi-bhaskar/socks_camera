#include "unicam.h"
#include "lib.h"
#include "interrupt.h"
#include "mailbox_interface.h"
#include "sys_timer.h"
#include "mmu.h"

#include <stddef.h>

#define UNICAM_REG(off) (UNICAM_BASE + (off))

// TODO: make GPIO clock in rpi_os
#define CM_CAM1CTL      (CAM_CLK_BASE + 0x48)
#define CM_CAM1DIV      (CAM_CLK_BASE + 0x4C)
#define CM_PASSWD       0x5A000000
#define CSI1_CLKGATE    (0x20802004 | KERNEL_VBASE)

#define CLK_TIMEOUT 1000000

static UnicamConfig g_cfg;
static bool g_active;

#define SWITCH_DMA_BUF(addr) \
    do { \
        uint32_t dma_addr = __pa(addr); \
        PUT32(UNICAM_REG(UNICAM_IBSA0), dma_addr); \
        PUT32(UNICAM_REG(UNICAM_IBEA0), dma_addr + g_cfg.buffer_size); \
    } while(0)

static bool start_cam_clock() { // set CAM1 clock to 100MHz
    mem_barrier_dsb();

    // reset clock & wait for busy bit
    PUT32(CM_CAM1CTL, CM_PASSWD | (1 << 5));
    for (int i = 0; i < CLK_TIMEOUT && (GET32(CM_CAM1CTL) & (1 << 7)); i++);

    // set divider to 5 and source to PLLD (fixed 500MHz)
    PUT32(CM_CAM1DIV, CM_PASSWD | (5 << 12));
    PUT32(CM_CAM1CTL, CM_PASSWD | (1 << 4) | 6);
    for (int i = 0; i < CLK_TIMEOUT; i++) {
        if (GET32(CM_CAM1CTL) & (1 << 7)) {
            printk("unicam: CAM1 clock started\n");
            mem_barrier_dsb();
            return true;
        }
    }
    printk("unicam: CAM1 clock failed\n");
    return false;
}
static void set_field(uint32_t* val, uint32_t field, uint32_t mask) {
    uint32_t m = mask;
    while (m && !(m & 1)) {
        field <<= 1;
        m >>= 1;
    }
    *val = (*val & ~mask) | (field & mask);
}
static void write_field(uint32_t reg, uint32_t field, uint32_t mask) {
    uint32_t v = GET32(reg);
    set_field(&v, field, mask);
    PUT32(reg, v);
}
static bool set_power(bool on) {
    assert(mbox_get_property_batch(5,
        MBOX_TAG_UNICAM, 8, 8, 14, (on ? 1 : 0)
    ), "Unicam mailbox command failed");
    
    if (on) {
        mbox_set_clock_rate(MBOX_CLK_ISP, 250000000);
    }
    return true;
}
static void clock_write(uint32_t lanes) {
    mem_barrier_dsb();
    PUT32(CSI1_CLKGATE, CM_PASSWD | lanes);
    mem_barrier_dsb();
}
static void stop_cam_clock() {
    mem_barrier_dsb();
    PUT32(CM_CAM1CTL, CM_PASSWD | (1 << 5));
    while (GET32(CM_CAM1CTL) & (1 << 7));
    mem_barrier_dsb();
}

volatile bool g_frame_waiting;
void __attribute__((interrupt("IRQ"))) unicam_irq_handler() {
    // static uint32_t lst_t;
    // check pending interrupt on CAM1
    if (GET32(IRQ_PENDING_2) & (1U << (CAM1_INT - 32))) {
        if (!g_active) return;

        mem_barrier_dsb();
        uint32_t sta = GET32(UNICAM_REG(UNICAM_STA));
        uint32_t ista = GET32(UNICAM_REG(UNICAM_ISTA));

        // clear interrupts
        PUT32(UNICAM_REG(UNICAM_STA), sta);
        PUT32(UNICAM_REG(UNICAM_ISTA), ista);

        if (!(sta & (UNICAM_IS | UNICAM_PI0))) return;

        // on frame end interrupt, advance + update DMA buffer
        if ((ista & UNICAM_FEI) || (sta & UNICAM_PI0)) {
            CameraBuffer* buf = camera_buffer_advance(g_frame_waiting);
            SWITCH_DMA_BUF(buf->buf);
            g_frame_waiting = false;

            // if (lst_t != 0) printk("i %d\n", sys_timer_get_usec() - lst_t);
            // lst_t = sys_timer_get_usec();
            mmu_flush_dcache();
        }

        mem_barrier_dsb();
    }
}

bool unicam_init() {
    if (!set_power(true)) {
        return false;
    }
    sys_timer_delay_us(1000);

    // enable CAM1 interrupts
    mem_barrier_dsb();
    PUT32(IRQ_ENABLE_2, 1U << (CAM1_INT - 32));
    mem_barrier_dsb();

    // install interrupt handler
    extern uint32_t irq_ptr;
    irq_ptr = (uint32_t) unicam_irq_handler;

    return true;
}

void unicam_deinit() {
    if (g_active) unicam_stop();
    set_power(false);
}

bool unicam_configure(UnicamConfig* cfg) {
    if (!cfg) return false;

    g_cfg = *cfg;
    if (g_cfg.stride == 0) {
        g_cfg.stride = (g_cfg.width * (g_cfg.depth == 10 ? 2 : 1) + 31) & ~31;
    }

    return true;
}

bool unicam_start() {
    if (g_active) return false;
    if (!start_cam_clock()) return false;

    clock_write(0b010101);
    mem_barrier_dsb();

    PUT32(UNICAM_REG(UNICAM_CTRL), UNICAM_MEM);

    uint32_t ana = UNICAM_AR;
    set_field(&ana, 7, 0xF0);
    set_field(&ana, 7, 0xF00);
    PUT32(UNICAM_REG(UNICAM_ANA), ana);
    sys_timer_delay_us(1000);

    write_field(UNICAM_REG(UNICAM_ANA), 0, UNICAM_AR);
    write_field(UNICAM_REG(UNICAM_CTRL), 1, UNICAM_CPR);
    write_field(UNICAM_REG(UNICAM_CTRL), 0, UNICAM_CPR);
    write_field(UNICAM_REG(UNICAM_CTRL), 0, UNICAM_CPE);

    uint32_t ctrl = GET32(UNICAM_REG(UNICAM_CTRL));
    set_field(&ctrl, 0, 1 << 3);
    set_field(&ctrl, 0, 1 << 5);
    set_field(&ctrl, 0xF, 0xF00);
    set_field(&ctrl, 128, 0x1FF000);
    PUT32(UNICAM_REG(UNICAM_CTRL), ctrl);

    PUT32(UNICAM_REG(UNICAM_IHWIN), 0);
    PUT32(UNICAM_REG(UNICAM_IVWIN), 0);

    uint32_t pri = GET32(UNICAM_REG(UNICAM_PRI));
    set_field(&pri, 0, 0x30000);
    set_field(&pri, 0, 0xF000);
    set_field(&pri, 0xE, 0xF00);
    set_field(&pri, 8, 0xF0);
    set_field(&pri, 2, 0x6);
    set_field(&pri, 1, 0x1);
    PUT32(UNICAM_REG(UNICAM_PRI), pri);

    write_field(UNICAM_REG(UNICAM_ANA), 0, UNICAM_DDL);

    uint32_t line_int = g_cfg.height >> 2;
    if (line_int < 128) line_int = 128;
    uint32_t ictl = UNICAM_FSIE | UNICAM_FEIE | UNICAM_IBOB;
    set_field(&ictl, line_int, 0x1FFF0000);
    PUT32(UNICAM_REG(UNICAM_ICTL), ictl);

    PUT32(UNICAM_REG(UNICAM_STA), 0x1FFFFFF);
    PUT32(UNICAM_REG(UNICAM_ISTA), 0x7);

    write_field(UNICAM_REG(UNICAM_CLT), 2, 0xFF);
    write_field(UNICAM_REG(UNICAM_CLT), 6, 0xFF00);
    write_field(UNICAM_REG(UNICAM_DLT), 2, 0xFF);
    write_field(UNICAM_REG(UNICAM_DLT), 6, 0xFF00);
    write_field(UNICAM_REG(UNICAM_DLT), 0, 0xFF0000);
    write_field(UNICAM_REG(UNICAM_CTRL), 0, UNICAM_SOE);

    uint32_t cmp0 = 0;
    set_field(&cmp0, 1, UNICAM_PCE);
    set_field(&cmp0, 1, UNICAM_GI);
    set_field(&cmp0, 1, UNICAM_CPH);
    set_field(&cmp0, 0, 0xC0);
    set_field(&cmp0, 1, 0x3F);
    PUT32(UNICAM_REG(UNICAM_CMP0), cmp0);

    uint32_t clk = 0;
    set_field(&clk, 1, UNICAM_CLE);
    set_field(&clk, 1, UNICAM_CLLPE);
    PUT32(UNICAM_REG(UNICAM_CLK), clk);

    uint32_t dat = 0;
    set_field(&dat, 1, UNICAM_DLE);
    set_field(&dat, 1, UNICAM_DLLPE);
    PUT32(UNICAM_REG(UNICAM_DAT0), dat);
    PUT32(UNICAM_REG(UNICAM_DAT1), dat);

    PUT32(UNICAM_REG(UNICAM_IBLS), g_cfg.stride);

    CameraBuffer* buf = camera_buffer_get_write();
    SWITCH_DMA_BUF(buf->buf);

    uint32_t unpack = (g_cfg.depth == 10) ? UNICAM_PUM_UNPACK10 : UNICAM_PUM_NONE;
    uint32_t pack = (g_cfg.depth == 10) ? UNICAM_PPM_PACK16 : UNICAM_PPM_NONE;
    uint32_t ipipe = 0;
    set_field(&ipipe, unpack, 0x7);
    set_field(&ipipe, pack, 0x380);
    PUT32(UNICAM_REG(UNICAM_IPIPE), ipipe);

    uint8_t dt = (g_cfg.depth == 10) ? CSI2_DT_RAW10 : CSI2_DT_RAW8;
    PUT32(UNICAM_REG(UNICAM_IDI0), (0 << 6) | dt);

    uint32_t misc = GET32(UNICAM_REG(UNICAM_MISC));
    set_field(&misc, 1, UNICAM_FL0);
    set_field(&misc, 1, UNICAM_FL1);
    PUT32(UNICAM_REG(UNICAM_MISC), misc);

    PUT32(UNICAM_REG(UNICAM_DCS), 0);

    write_field(UNICAM_REG(UNICAM_CTRL), 1, UNICAM_CPE);
    write_field(UNICAM_REG(UNICAM_ICTL), 1, 0x60);

    mem_barrier_dsb();
    sys_timer_delay_us(10000);
    
    g_active = true;
    printk("done init\n");
    return true;
}

void unicam_stop() {
    if (!g_active) return;
    mem_barrier_dsb();

    write_field(UNICAM_REG(UNICAM_ANA), 1, UNICAM_DDL);
    write_field(UNICAM_REG(UNICAM_CTRL), 1, UNICAM_SOE);
    PUT32(UNICAM_REG(UNICAM_DAT0), 0);
    PUT32(UNICAM_REG(UNICAM_DAT1), 0);
    write_field(UNICAM_REG(UNICAM_CTRL), 1, UNICAM_CPR);
    sys_timer_delay_us(50);
    write_field(UNICAM_REG(UNICAM_CTRL), 0, UNICAM_CPR);
    write_field(UNICAM_REG(UNICAM_CTRL), 0, UNICAM_CPE);

    mem_barrier_dsb();
    clock_write(0);
    stop_cam_clock();
    g_active = false;
}

bool unicam_wait_frame() {
    for (g_frame_waiting = true; g_frame_waiting; );
    return true;
}
