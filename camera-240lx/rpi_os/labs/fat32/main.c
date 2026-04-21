#include "fat.h"
#include "emmc.h"
#include "uart.h"
#include "lib.h"
#include "debug.h"

/* 8.3 filename: "NAME    EXT" (8 chars name, space-padded, then 3 chars extension) */
// #define FILE_NAME "FLY     PLY"
#define FILE_NAME "CONFIG  TXT"

int main(void) {
    fat_init();
    printk("init done!\n");

    uint8_t* data;
    uint32_t filesize;
    fat_read_file(FILE_NAME, &data, &filesize);
    if (data == 0) {
        printk("ERROR: Failed to read file\n");
        rpi_reset();
    }

    printk("File size: %d\n", filesize);

    printk("File read OK. First 64 bytes:\n");
    for (unsigned int i = 0; i < 64 && i < filesize && data[i] != '\0'; i++) {
        printk("%c", (unsigned char) data[i]);
    }

    printk("\nLast 64 bytes of file:\n");
    // Use actual file_size for last bytes
    unsigned int end = filesize;
    unsigned int start = (end > 64) ? (end - 64) : 0;
    for (unsigned int i = start; i < end; i++) {
        printk("%c", (unsigned char) data[i]);
    }
    printk("\n\nDone.\n");

    /*
    const char* vertex_count = "vertex ";
    const char* end_header = "end_header\n";

    int st = 0;
    for (; ; st++) {
        int f = 0;
        for (int j = 0; j < 7; j++) {
            if (data[st + j] != vertex_count[j]) {
                f = 1;
                break;
            }
        }

        if (!f) {
            break;
        }
    }
    st += 7;
    uint32_t cnt = 0;
    while (data[st] != '\n') {
        cnt = cnt * 10 + (data[st++] - '0');
    }

    DEBUG_D(cnt);

    for (; ; st++) {
        int f = 0;
        for (int j = 0; j < 11; j++) {
            if (data[st + j] != end_header[j]) {
                f = 1;
                break;
            }
        }

        if (!f) {
            break;
        }
    }

    st += 11;

    for (int i = 0; i < 10; i++) {
        Gaussian g;
        memcpy(&g, data + st, sizeof(Gaussian));
        DEBUG_F(g.pos_x);
        DEBUG_F(g.pos_y);
        DEBUG_F(g.pos_z);

        st += sizeof(Gaussian);
    }

    */
    rpi_reset();
    return 0;
}
