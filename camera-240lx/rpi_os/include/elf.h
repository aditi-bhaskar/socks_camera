#include <stdint.h>

#define ELF_MAGIC 0x464C457F // '\x7F', 'E', 'L', 'F' in little-endian
#define PT_LOAD   1          // segment to load into RAM

#define ELF_LSB 1
#define ELF_ARM 40

typedef struct {
    uint32_t e_magic;     // Must be ELF_MAGIC
    uint8_t  e_class;     // 1 = 32-bit, 2 = 64-bit
    uint8_t  e_data;      // 1 = little Endian, 2 = big Endian
    uint8_t  e_version;
    uint8_t  e_osabi;
    uint8_t  e_abiversion;
    uint8_t  e_pad[7];
    uint16_t e_type;      // 2 = executable
    uint16_t e_machine;   // 40 = ARM
    uint32_t e_version2;
    uint32_t e_entry;     // entry point
    uint32_t e_phoff;     // file offset to program headers
    uint32_t e_shoff;     // file offst to section headers
    uint32_t e_flags;
    uint16_t e_ehsize;    // elf header size (52 bytes)
    uint16_t e_phentsize; // program header size (32 bytes)
    uint16_t e_phnum;     // # of prog headers
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;      // we only care about PT_LOAD (1)
    uint32_t p_offset;    // where in the file is the data?
    uint32_t p_vaddr;     // where in virtual memory should we put it?
    uint32_t p_paddr;     // (ignore)
    uint32_t p_filesz;    // how many bytes to read from the file
    uint32_t p_memsz;     // how much RAM to allocate
    uint32_t p_flags;     // 1=Execute, 2=Write, 4=Read
    uint32_t p_align;
} Elf32_Phdr;
