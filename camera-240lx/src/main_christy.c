// main_christy.c 
// this runs on christy's pi: it receives frames over BT and forwards the bytes to
// the laptop UART. no camera/oled/buttons for this pi. 
// once forwarding starts we print nothing: since printk shares the laptop UART and would corrupt the image stream.

#include "rpi.h"
#include "bt.h"

#define BT_LOCAL_NAME   "RPI Christy"
#define BT_PEER_ADDR_BE { 0xb8, 0x27, 0xeb, 0x0b, 0x88, 0x64 }   // aditi, big-endian
#define BT_BAUD         3000000

// laptop UART baud. NOTE: must match laptop-side-display.py
// divisor 10 -> 250e6 / ((10+1)*8) = 2840909 baud
#define LAPTOP_UART_BAUD 2840909
#define LAPTOP_UART_DIV  10

// debug mode allows us to printk for the bt packet stats; does not forward the bt bytes to laptop
#define CHRISTY_DEBUG 0

void notmain() {
    printk("\n[christy] starting (acceptor, bt_baud=%d laptop_baud=%d, mode=%s)\n",
           BT_BAUD, LAPTOP_UART_BAUD, CHRISTY_DEBUG ? "DEBUG" : "PRODUCTION");

    static const uint8_t peer_addr_be[] = BT_PEER_ADDR_BE;

    // init uart
    uart_init();
    PUT32(0x20215068, LAPTOP_UART_DIV);   // mini-UART baud divisor
    bt_setup(BT_LOCAL_NAME, BT_BAUD);
    delay_ms(2000);

    printk("[christy] waiting for connection from aditi...\n");
    uint16_t handle = bt_connect_acceptor(peer_addr_be);
    printk("[christy] connected, handle=%d\n", handle);

#if CHRISTY_DEBUG
    // receives a full frame and prints a summary, 
    // does not forward
    // christy's pi must be plugged into the laptop so printk is visible
    #define MAX_FRAME_BYTES (12 + 640 * 480 * 3)
    uint8_t *frame_buf = kmalloc(MAX_FRAME_BYTES);
    uint32_t total = 0, npkts = 0;

    while (total < 12) {
        struct hci_acl_data_pkt *pkt = bt_receive_acl();
        npkts++;
        for (int i = 0; i < pkt->data_len && total < MAX_FRAME_BYTES; i++)
            frame_buf[total++] = pkt->data[i];
    }

    uint32_t magic = ((uint32_t)frame_buf[0] << 24) | ((uint32_t)frame_buf[1] << 16)
                   | ((uint32_t)frame_buf[2] <<  8) |  (uint32_t)frame_buf[3];
    uint32_t w = ((uint32_t)frame_buf[4] << 24) | ((uint32_t)frame_buf[5] << 16)
                   | ((uint32_t)frame_buf[6] <<  8) |  (uint32_t)frame_buf[7];
    uint32_t h = ((uint32_t)frame_buf[8] << 24) | ((uint32_t)frame_buf[9] << 16)
                   | ((uint32_t)frame_buf[10]<<  8) |  (uint32_t)frame_buf[11];
    printk("[christy] header: magic=0x%X w=%d h=%d\n", magic, w, h);

    uint32_t frame_size = 12 + w * h * 3;
    while (total < frame_size) {
        struct hci_acl_data_pkt *pkt = bt_receive_acl();
        npkts++;
        for (int i = 0; i < pkt->data_len && total < frame_size; i++){
            frame_buf[total++] = pkt->data[i];
        }
        if (npkts % 10 == 0){
            printk("[christy]   ... %d pkts, %d / %d bytes\n", npkts, total, frame_size);
        }
    }

    printk("[christy] done: %d bytes in %d packets\n", total, npkts);
    printk("[christy] first pixel RGB: %d %d %d\n",
           frame_buf[12], frame_buf[13], frame_buf[14]);
#else
    // we are not in debug mode; so forward along the packets!!
    #define HDR_LEN 20
    #define MAX_FRAME_BYTES (HDR_LEN + 640 * 480 * 3)
    // drop a partial frame if no bytes arrive for this long (sender died mid-frame).
    #define RX_STALL_TIMEOUT_US 5000000u // bluetooth timeout = 5 seconds if we dont receive the full packet
    uint8_t *frame_buf = kmalloc(MAX_FRAME_BYTES);

    while (1) {
        // phase 1: we read the whole frame into RAM as fast as possible. 
        // uart_put8 is much slower than the 3Mbaud BT link, so forwarding
        //  mid-receiv overflows the bluetooth's RX FIFO and drops packets
        uint32_t total = 0;
        uint32_t last_rx = timer_get_usec();
        int aborted = 0;

        while (total < HDR_LEN) {
            struct hci_acl_data_pkt *pkt = bt_receive_acl_async();
            // checking packet and updating
            if (pkt) {
                for (int i = 0; (i < pkt->data_len) && (total < MAX_FRAME_BYTES); i++){
                    frame_buf[total++] = pkt->data[i];
                }
                last_rx = timer_get_usec();
            } else if (total > 0 && (timer_get_usec() - last_rx > RX_STALL_TIMEOUT_US)) {
                aborted = 1; break;
            }
        }
        if (aborted) continue;

        // payload_len = 5th big-endian u32 
        // this is bytes 16 to 19 inclusive
        uint32_t payload_len = ((uint32_t)frame_buf[16] << 24) | ((uint32_t)frame_buf[17] << 16)
            | ((uint32_t)frame_buf[18] <<  8) |  (uint32_t)frame_buf[19];
        uint32_t frame_size = HDR_LEN + payload_len;
        if (frame_size > MAX_FRAME_BYTES) {
            frame_size = MAX_FRAME_BYTES;
        }

        while (total < frame_size) {
            struct hci_acl_data_pkt *pkt = bt_receive_acl_async();
            if (pkt) {
                for (int i = 0; i < pkt->data_len && total < frame_size; i++){
                    frame_buf[total++] = pkt->data[i];\
                }
                last_rx = timer_get_usec();
            } else if (timer_get_usec() - last_rx > RX_STALL_TIMEOUT_US) {
                aborted = 1; break;
            }
        }
        if (aborted) {
            continue;
        }

        // phase 2: forward the buffered frame to laptop over uart. 
        // printk here would corrupt the stream the laptop is reading.
        uint32_t fwd_t0 = timer_get_usec();
        for (uint32_t i = 0; i < frame_size; i++){
            uart_put8(frame_buf[i]);
        }

        // when it is safe to print between frames: the laptop is hunting for the next "magic".
        uint32_t fwd_ms = (timer_get_usec() - fwd_t0) / 1000;
        uint32_t kbs = fwd_ms ? (frame_size * 1000u / fwd_ms) / 1024u : 0;
        printk("\n[perf] UART fwd: %d bytes in %d ms = %d KB/s\n", (int)frame_size, (int)fwd_ms, (int)kbs);
    }
#endif
}
