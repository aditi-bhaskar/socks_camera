#include "mmu.h"
#include "lib.h"
#include "uart.h"
#include "sys_timer.h"

#define DEBUG
#include "debug.h"

void main() {
    CP15CtrlReg r = get_cp15_ctrl_reg();
    DEBUG_D(r.XP);
    DEBUG_D(r.mmu_enbl);

    page_table_init();

    mmu_init();
    r = get_cp15_ctrl_reg();
    DEBUG_D(r.XP);
    DEBUG_D(r.mmu_enbl);

    mmu_enable();
    r = get_cp15_ctrl_reg();
    DEBUG_D(r.XP);
    DEBUG_D(r.mmu_enbl);
    DEBUG_D(r.icache_enbl);

    const int N = 10000;
    int a[N], b[N], c[N];
    for (int i = 0; i < N; i++) {
        a[i] = i;
    }

    uint32_t t = sys_timer_get_usec();
    for (int i = 0; i < N; i++) {
        b[i] = a[i] + a[i];
    }
    uint32_t tt = sys_timer_get_usec() - t;
    DEBUG_D(tt);

    mmu_enable_caches();
    t = sys_timer_get_usec();
    for (int i = 0; i < N; i++) {
        c[i] = a[i] + a[i];
    }
    tt = sys_timer_get_usec() - t;
    DEBUG_D(tt);

    for (int i = 0; i < N; i++) {
        assert(b[i] == c[i], "mismatch\n");
    }

    mmu_flush_dcache();

    rpi_reset();
}
