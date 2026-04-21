// 6-aditi-custom.c
//
// One configurable BT test for BOTH Pis. Flip the #defines in the CONFIG
// block below, reflash, and go. Same binary logic for initiator & acceptor;
// only ROLE changes who dials vs. who waits.
//
// Quick start:
//   1. Flash 4-read-bdaddr.c on each Pi, note the printed "BD_ADDR:" line.
//   2. On Pi A: ROLE = ROLE_ACCEPTOR, PEER_ADDR_BE = Pi B's address.
//   3. On Pi B: ROLE = ROLE_INITIATOR, PEER_ADDR_BE = Pi A's address.
//   4. Flash the ACCEPTOR first (it waits), then the INITIATOR (it dials).

#include "bt.h"
#include "demand.h"
#include "hci-consts.h"
#include "pl011.h"
#include "rpi.h"
#include <string.h>

// ===========================================================================
//                                  CONFIG
//             (edit these — valid values are noted on each line)
// ===========================================================================

#define ROLE_INITIATOR 0   // "sender of the call": dials out (CREATE_CONNECTION)
#define ROLE_ACCEPTOR  1   // "receiver of the call": waits (SCAN_ENABLE + ACCEPT)

// >>> Who is THIS Pi? valid: ROLE_INITIATOR | ROLE_ACCEPTOR
#define ROLE            ROLE_INITIATOR

// >>> This Pi's friendly name. valid: any string up to 247 chars
#define LOCAL_NAME      "RPI Aditi"

// >>> The OTHER Pi's BD_ADDR, big-endian, exactly as 4-read-bdaddr prints it
//     ("aa:bb:cc:dd:ee:ff" -> { 0xaa,0xbb,0xcc,0xdd,0xee,0xff }).
//     valid: the 6 bytes of the peer's real hardware address
// #define PEER_ADDR_BE    { 0x1e, 0x70, 0x01, 0x14, 0x68, 0x05 }
#define PEER_ADDR_BE    { 0xb8, 0x27, 0xeb, 0x30, 0xd1, 0x77 } // Christy bt addr

// >>> UART (Pi <-> BT chip) baud. valid: 115200 (default/safe),
//     921600, 1500000, 3000000. Anything != 115200 sends the Broadcom
//     UPDATE_BAUDRATE vendor command to the chip, then reprograms the PL011.
#define TARGET_BAUD     3000000   // 115200

// >>> How long the INITIATOR keeps paging the peer before the chip gives up
//     and returns a failed CONNECTION_COMPLETE (status 0x04 = Page Timeout).
//     Units are 0.625ms each. valid: 0x0001..0xFFFF (0xFFFF ~= 40.9s).
//     Default chip value is 0x2000 (~5.1s); bump this up if the peer needs
//     more time to come online / start page-scanning.
#define PAGE_TIMEOUT    0xFFFF

// >>> Send a fixed message once, immediately after connecting? valid: 0 | 1
#define AUTO_SEND_ON_CONNECT  1
// >>> The message to auto-send. valid: any string
#define AUTO_SEND_MSG   "hello from " LOCAL_NAME "\r\n"

// ===========================================================================
//                              END CONFIG
// ===========================================================================

static u8 peer_addr_be[] = PEER_ADDR_BE;

// Send one ACL data packet (flags = 0) on the given connection handle.
static void send_packet(u8 handle[2], const u8 *data, u16 len) {
    struct hci_acl_data_pkt acl = {0};
    memcpy(&acl.handle, handle, 2);
    if (len > sizeof(acl.data))
        len = sizeof(acl.data);
    acl.data_len = len;
    memcpy(acl.data, data, len);
    bt_send_acl_data(&acl);
}

// Tell the BT chip to switch UART baud, then move the Pi side to match.
static void switch_baud(unsigned baud) {
    if (baud == 115200)
        return; // chip boots at 115200; nothing to do.

    printk("Switching UART baud to %d...\n", baud);

    struct hci_command_pkt cmd = {0};
    cmd.opcode = CMD_BCM_UPDATE_BAUDRATE;
    cmd.params_len = 6;
    cmd.params[0] = 0x00; // 2 reserved bytes
    cmd.params[1] = 0x00;
    cmd.params[2] = baud & 0xff; // baud, little-endian
    cmd.params[3] = (baud >> 8) & 0xff;
    cmd.params[4] = (baud >> 16) & 0xff;
    cmd.params[5] = (baud >> 24) & 0xff;
    bt_send_command(&cmd);

    // The chip ACKs at the OLD baud, THEN switches its own UART.
    struct hci_event_pkt *evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0); // chip accepted the new baud

    // Let the chip finish flipping its UART, then match it on the Pi side.
    delay_ms(10);
    pl011_set_baud(baud);
    delay_ms(10);
    printk("Baud switched to %d.\n", baud);
}

void notmain(void) {
    bt_init();
    bt_upload_firmware();

    // Optionally run the whole link faster than the 115200 boot default.
    switch_baud(TARGET_BAUD);

    u8 peer_addr_le[6];
    for (int i = 0; i < 6; i++)
        peer_addr_le[i] = peer_addr_be[5 - i];

    struct hci_command_pkt cmd = {0};
    struct hci_event_pkt *evt;
    struct hci_acl_data_pkt *acl_recv;

    // ---- Read our own BD_ADDR --------------------------------------------
    cmd.opcode = CMD_READ_BD_ADDR;
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params_len == 10);
    assert(evt->params[3] == 0);

    printk("BD_ADDR: ");
    for (int i = 5; i >= 0; i--) {
        u8 b = evt->params[4 + i];
        printk("%X%X", b >> 4, b & 0xf); // printk has no %02x padding support
        if (i != 0)
            printk(":");
    }
    printk("\n");

    // ---- Set our friendly name -------------------------------------------
    cmd.opcode = CMD_WRITE_LOCAL_NAME;
    cmd.params_len = 248;
    memset(cmd.params, 0, sizeof(cmd.params));
    memcpy(cmd.params, LOCAL_NAME, sizeof(LOCAL_NAME));
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0);

    // ---- Read max ACL buffer sizes (informational) -----------------------
    cmd.opcode = CMD_READ_BUFFER_SIZE;
    cmd.params_len = 0;
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 11);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0);
    printk("Max ACL data packet length: %d\n", evt->params[4] | (evt->params[5] << 8));
    printk("Max number ACL data packets: %d\n", evt->params[7] | (evt->params[8] << 8));

    u8 handle[2];

#if ROLE == ROLE_ACCEPTOR
    // ---- ACCEPTOR: make ourselves pageable, then wait for a call --------
    cmd.opcode = CMD_WRITE_SCAN_ENABLE;
    cmd.params_len = 1;
    cmd.params[0] = 0x02; // page scan only (accept incoming connections)
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0);

    printk("Waiting for connection...\n");

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_CONNECTION_REQUEST);
    assert(evt->params_len == 10);
    assert(memcmp(&evt->params[0], peer_addr_le, 6) == 0); // it's our peer
    assert(evt->params[9] == 1); // ACL link

    cmd.opcode = CMD_ACCEPT_CONNECTION_REQUEST;
    cmd.params_len = 7;
    memcpy(&cmd.params[0], peer_addr_le, 6);
    cmd.params[6] = 0x01; // stay slave (don't request role switch)
    bt_send_command(&cmd);

    // Async command -> COMMAND_STATUS (not COMMAND_COMPLETE).
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_STATUS);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[2], &cmd.opcode, 2) == 0);
    // COMMAND_STATUS params: [0]=status [1]=n_cmds [2-3]=opcode
    assert(evt->params[0] == 0); // pending OK

#else // ROLE_INITIATOR
    // ---- INITIATOR: give ourselves longer to reach the peer -------------
    // CREATE_CONNECTION pages the peer for up to PAGE_TIMEOUT (units of
    // 0.625ms); past that, the chip returns CONNECTION_COMPLETE with status
    // 0x04 (Page Timeout). Bump it up so a slow-to-appear peer still connects.
    cmd.opcode = CMD_WRITE_PAGE_TIMEOUT;
    cmd.params_len = 2;
    cmd.params[0] = PAGE_TIMEOUT & 0xff;        // little-endian
    cmd.params[1] = (PAGE_TIMEOUT >> 8) & 0xff;
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0);

    // ---- INITIATOR: dial the peer ---------------------------------------
    cmd.opcode = CMD_CREATE_CONNECTION;
    cmd.params_len = 13;
    memcpy(&cmd.params[0], peer_addr_le, 6); // BD_ADDR (little-endian)
    cmd.params[6]  = 0x18; // packet types 0xcc18 = allow all classic
    cmd.params[7]  = 0xcc;
    cmd.params[8]  = 0x02; // page scan repetition mode R2
    cmd.params[9]  = 0x00; // reserved
    cmd.params[10] = 0x00; // clock offset (none)
    cmd.params[11] = 0x00;
    cmd.params[12] = 0x00; // no role switch
    bt_send_command(&cmd);

    // Async command -> COMMAND_STATUS (not COMMAND_COMPLETE).
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_STATUS);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[2], &cmd.opcode, 2) == 0);
    assert(evt->params[0] == 0); // pending OK
#endif

    // ---- Both roles: wait for CONNECTION_COMPLETE ------------------------
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_CONNECTION_COMPLETE);
    // params: [0]=status [1-2]=handle [3-8]=BD_ADDR [9]=link_type [10]=enc
    assert(evt->params_len == 11);
    assert(evt->params[0] == 0); // status success
    assert(evt->params[9] == 1); // ACL link
    handle[0] = evt->params[1];  // LSB
    handle[1] = evt->params[2];  // MSB

    printk("Connected! Handle: %X%X%X%X\n",
        handle[1] >> 4, handle[1] & 0xf, handle[0] >> 4, handle[0] & 0xf);

#if AUTO_SEND_ON_CONNECT
    for (int i = 0; i < 100; i++) {
        send_packet(handle, (const u8 *)AUTO_SEND_MSG, sizeof(AUTO_SEND_MSG) - 1);
        printk("Auto-sent: %s", AUTO_SEND_MSG);
    }
#endif

    // ---- Bidirectional bridge: laptop keyboard <-> BT link ---------------
    while (1) {
        evt = bt_receive_event_async();
        if (evt) {
            if (evt->event_code != EVENT_NUMBER_OF_COMPLETED_PACKETS) {
                printk("Received event code %X\n", evt->event_code);
                if (evt->event_code == EVENT_DISCONNECTION_COMPLETE) {
                    printk("\nDisconnected\n");
                    break;
                }
            }
        }

        // BT -> laptop
        acl_recv = bt_receive_acl_async();
        if (acl_recv) {
            for (int i = 0; i < acl_recv->data_len; i++)
                uart_put8(acl_recv->data[i]);
        }

        // laptop -> BT
        if (uart_has_data()) {
            u8 buf[1021];
            u16 len = 0;
            while (uart_has_data() && len < sizeof(buf))
                buf[len++] = uart_get8();
            send_packet(handle, buf, len);
        }
    }
}
