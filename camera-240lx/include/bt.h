// Javier Garcia Nieto <jgnieto@cs.stanford.edu>. CS340LX Fall 2025.

#ifndef BT_H
#define BT_H

#include <stdint.h>

#define BT_EN   45

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define OPCODE(ogf, ocf) (((ocf) & 0x03FF) | (((ogf) & 0x3F) << 10))
#define OGF(opcode) ((opcode) >> 10)
#define OCF(opcode) ((opcode) & 0x03FF)

enum bt_packet_type {
    HCI_CMD =       0x01,
    HCI_ACL_DATA =  0x02,
    HCI_SCO_DATA =  0x03,
    HCI_EVENT =     0x04,
};

struct hci_command_pkt {
    u16 opcode;
    u8 params_len;
    u8 params[255];
};

struct hci_event_pkt {
    u8 event_code;
    u8 params_len;
    u8 params[255];
};

struct hci_acl_data_pkt {
    u16 handle;
    u16 data_len;
    u8 data[1021]; // BT max is 2^16 but our BT chip has a max of 1021
};

// Low-level primitives
void bt_init(void);
void bt_upload_firmware(void);
void bt_send_command(struct hci_command_pkt *cmd);
void bt_send_acl_data(struct hci_acl_data_pkt *acl);

struct hci_event_pkt *bt_receive_event(void);
struct hci_event_pkt *bt_receive_event_async(void);

struct hci_acl_data_pkt *bt_receive_acl(void);
struct hci_acl_data_pkt *bt_receive_acl_async(void);

// High-level connection API

// Switch both the BCM chip and the Pi PL011 to a higher baud rate.
// Call after bt_init() + bt_upload_firmware(), before any HCI traffic.
// Valid rates: 115200 (no-op / safe default), 921600, 1500000, 3000000.
// bt_setup() calls this internally when baud != 115200.
void bt_set_baud(unsigned baud);

// Full bringup: init, firmware, optional baud switch (pass 115200 to skip),
// read+print BD_ADDR, set local name, print buffer sizes.
void bt_setup(const char *local_name, unsigned baud);

// Dial out to peer (big-endian addr). page_timeout units = 0.625ms each;
// 0xFFFF (~40.9s) prevents spurious Page Timeout failures.
// Returns ACL handle on success; asserts on chip error.
u16  bt_connect_initiator(const u8 peer_addr_be[6], u16 page_timeout);

// Enable page scanning and wait for peer to call in (big-endian addr).
// Returns ACL handle on success; asserts on chip error.
u16  bt_connect_acceptor(const u8 peer_addr_be[6]);

// Send one ACL data packet on the given connection handle.
void bt_send_packet(u16 handle, const u8 *data, u16 len);

// Send a packed-RGB framebuffer (0x00RRGGBB per pixel) as raw R,G,B bytes.
// No framing headers; receiver gets width*height*3 raw bytes.
void bt_send_image(u16 handle, const u32 *rgb, u32 width, u32 height);

// Send an opaque byte buffer (e.g. a compressed/RLE payload) over BT, chunked
// into ACL packets with the same credit-based flow control as bt_send_image.
// No framing headers; receiver gets exactly len raw bytes. Returns 1 on
// success, 0 if it timed out waiting for the peer (link dropped mid-send).
int bt_send_raw(u16 handle, const u8 *data, u32 len);


#endif // BT_H
