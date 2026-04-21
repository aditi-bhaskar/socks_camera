#ifndef PROCESS_H
#define PROCESS_H

#include "fd.h"
#include "vma.h"

#define PROC_HEAP_START 0x10000000
#define PROC_STACK_START 0xB0000000
#define KUSER_PAGE 0xFFFF0000

#define MAX_FDS 32
#define V6L 0x006c3676 // "v6l\0" in hex little endian

// ELF auxiliary wector types
enum {
    AT_NULL     = 0,
    AT_PAGESZ   = 6,
    AT_PLATFORM = 15,
    AT_HWCAP    = 16
};

// ARM hardware capabilities
enum {
    HWCAP_SWP   = (1 << 0),
    HWCAP_HALF  = (1 << 1),
    HWCAP_TLS   = (1 << 15),
};

typedef struct {
    uint32_t r[13];
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t cpsr;
} TrapFrame;

typedef enum {
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_DEAD
} ProcessState;

typedef struct {
    uint32_t pid;
    ProcessState state;
    TrapFrame context;
    uint32_t prog_break;

    uint32_t l1_pt_paddr;

    File* fd_table[MAX_FDS];
    VMArea* vma_list;
} Process;

extern Process* current_process;
Process* proc_create(const char* filename);
void proc_run(TrapFrame* context, uint32_t l1_pt_vaddr);
void proc_destroy(Process* p);

#endif
