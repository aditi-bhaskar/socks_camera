#include "lib.h"

void undefined_instruction_vector(uint32_t instr_addr) {
    printk("=============================\n");
    printk("[FATAL] UNDEFINED INSTRUCTION\n");
    printk("Instruction: %x\n", instr_addr);

    rpi_reset();
}

void __attribute__((interrupt("SWI"))) software_interrupt_vector() {
    printk("Unimplemented SWI vector\n");
    rpi_reset();
}

void prefetch_abort_vector(uint32_t instr_addr) {
    printk("=============================\n");
    printk("[FATAL] PREFETCH ABORT\n");
    printk("Instruction: %x\n", instr_addr);

    rpi_reset();
}

void data_abort_vector(uint32_t instr_addr, uint32_t dfsr, uint32_t fault_addr) {
    printk("=============================\n");
    printk("[FATAL] DATA ABORT\n");
    printk("Instruction: %x\n", instr_addr);
    printk("Memory address: %x\n", fault_addr);
    printk("Status: %x\n", dfsr);
    
    rpi_reset();
}

void __attribute__((interrupt("IRQ"))) interrupt_vector() {
    printk("Unimplemented interrupt vector\n");
    rpi_reset();
}

void __attribute__((interrupt("FIQ"))) fast_interrupt_vector() {
    printk("Unimplemented fast interrupt vector\n");
    rpi_reset();
}

void __attribute__((interrupt("RESET"))) reset_vector() {
    printk("Unimplemented reset vector\n");
    rpi_reset();
}
