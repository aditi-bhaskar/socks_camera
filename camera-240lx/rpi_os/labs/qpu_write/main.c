/*

Goal: Write to output array

Output is size N, ELEMS_PER_QPU = N / NUM_QPUS. We want

    output[i * ELEMS_PER_QPU : (i + 1) * ELEMS_PER_QPU] = i

for i = 0 ... NUM_QPUS - 1

QPU i is in charge of element i. It's given:
- Pointer to beginning index in output
- Number of 16-wide moves it has to do (ELEMS_PER_QPU / 16)
- i

*/

#include "mailbox_interface.h"
#include "qpu.h"
#include "lib.h"
#include "uart.h"
#include "sys_timer.h"
#include "arena_allocator.h"
#include "kernel.h"

#include "qpu_code.h"

#define NUM_QPUS 16
#define NUM_UNIFS 3
#define ITERATIONS 10000
#define SIMD_WIDTH 16
#define N (ITERATIONS * NUM_QPUS * SIMD_WIDTH)
#define ELEMS_PER_QPU (ITERATIONS * SIMD_WIDTH)

typedef struct {
    Arena arena;
    Kernel kernel;

    volatile uint32_t* output;
} GPU;

void main() {
    uint32_t t;

    uint32_t output_len = N * sizeof(uint32_t);

    GPU gpu;
    kernel_init(&gpu.kernel,
            NUM_QPUS, NUM_UNIFS,
            qpu_code, sizeof(qpu_code));

    arena_init(&gpu.arena, N * sizeof(uint32_t));
    gpu.output = arena_alloc_align(&gpu.arena, output_len, 16);

    for (uint32_t i = 0; i < NUM_QPUS; i++) {
        kernel_load_unif(&gpu.kernel, i, 0, TO_BUS(gpu.output) + i * ELEMS_PER_QPU * sizeof(uint32_t));
        kernel_load_unif(&gpu.kernel, i, 1, ITERATIONS / 4);
        kernel_load_unif(&gpu.kernel, i, 2, i);
    }

    mem_barrier_dsb();

    t = sys_timer_get_usec();
    kernel_execute(&gpu.kernel);
    uint32_t gpu_us = sys_timer_get_usec() - t;

    uart_puts("GPU time: ");
    uart_putd(gpu_us);
    uart_puts("us\n");

    uint32_t arr[N];

    t = sys_timer_get_usec();
    for (uint32_t i = 0, c = 0; i < NUM_QPUS; i++) {
        for (int j = 0; j < ELEMS_PER_QPU; j++) {
            arr[c++] = i;
        }
    }
    uint32_t cpu_us = sys_timer_get_usec() - t;

    uart_puts("CPU time: ");
    uart_putd(cpu_us);
    uart_puts("us\n");

    uint32_t speedup_1000x = 1000.f * cpu_us / gpu_us;
    uart_puts("Speedup: ");
    uart_putd(speedup_1000x / 1000);
    uart_puts(".");
    uart_putd(speedup_1000x - (speedup_1000x / 1000) * 1000);
    uart_puts("x\n");

    /*
    for (int i = 0; i < N; i++) {
        uart_putd(gpu.output[i]);
        uart_puts(" ");

        if (i % ELEMS_PER_QPU == ELEMS_PER_QPU - 1) {
            uart_puts("\n");
        }
    }
    */

    for (int i = 0; i < N; i++) {
        assert(arr[i] == gpu.output[i], "mismatch");
    }

    rpi_reset();
}
