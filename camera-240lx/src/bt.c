// Javier Garcia Nieto <jgnieto@cs.stanford.edu>. CS340LX Fall 2025.
// lab completed by aditi

#define CQE_T void*

#include "bt.h"
#include "hci-consts.h"
#include "rpi.h"
#include "pl011.h"
#include "circular.h"
#include "gpio-high.h"

#include <stdbool.h>
#include <string.h>

static struct {
    cq_t acl_rx_buffer;
    cq_t event_rx_buffer;
    bool initialized;

    // how many commands can we send without receiving events
    unsigned n_commands_can_send;
} module;

void bt_init(void) {
    assert(!module.initialized);

    module.initialized = true;
    module.n_commands_can_send = 1;

    // todo("Call init on the pl011 module.");
    pl011_init();

    cq_init(&module.acl_rx_buffer, true);
    cq_init(&module.event_rx_buffer, true);
    kmalloc_init(); // no free, so get a bunch of space // removed the 100 arg bc kmalloc init doesnt take an arg...

    // todo("Set the Bluetooth enable pin (BT_EN) to on and wait for 800ms. Remember to use gpio_hi.");
    // BT_EN is 45, which is > 31
    // recall: use the hi functions since gpio_set ignores pins over 31
    gpio_hi_set_function(BT_EN, GPIO_FUNC_OUTPUT);
    gpio_hi_set_on(BT_EN);
    delay_ms(800);
}

/*
 * HCI ACL Data:
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |         Handle        | Flags |       Data Total Length       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  |                              Data                             |
 *  |                               ...                             |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static void _receive_acl_data(struct hci_acl_data_pkt *acl) {
    assert(module.initialized);

    // Warning: make sure that you only take exactly the number of bytes
    // necessary.
    // todo("Process incoming ACL data packet in a blocking fashion");
    // handle[11:0] is connection handle
    // handle[15:12] is packet boundary + broadcast flags
    acl->handle  = (u16)pl011_get8() | ((u16)pl011_get8() << 8);
    acl->data_len = (u16)pl011_get8() | ((u16)pl011_get8() << 8);
    for (int i = 0; i < acl->data_len; i++) {
        acl->data[i] = pl011_get8();
    }
}

/*
 * HCI Event:
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Event Code  |   Params Len  |        Event Parameter 0      |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |        Event Parameter 1      | Evnt Param 2  | Evnt Param 3  |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                               ...                             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |      Event Parameter N-1      |       Event Parameter N       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static void _receive_event(struct hci_event_pkt *evt) {
    assert(module.initialized);

    // Warning: make sure that you only take exactly the number of bytes
    // necessary.
    // todo("Process incoming HCI event packet in a blocking fashion");
    evt->event_code = pl011_get8();
    evt->params_len = pl011_get8();
    for (int i = 0; i < evt->params_len; i++) {
        evt->params[i] = pl011_get8();
    }

    // The EVENT_COMMAND_COMPLETE and EVENT_COMMAND_STATUS events include flow
    // control information that tells us how many commands we can send until
    // we get another one of these events giving us permission to send more.
    if (evt->event_code == EVENT_COMMAND_COMPLETE && evt->params_len >= 1) {
        u8 n_cmds = evt->params[0];
        module.n_commands_can_send = n_cmds;
    } else if (evt->event_code == EVENT_COMMAND_STATUS && evt->params_len >= 2) {
        u8 n_cmds = evt->params[1];
        module.n_commands_can_send = n_cmds;
    }
}

// Blockingly receive a single packet.
static void _receive_packet(void) {
    assert(module.initialized);
    u8 packet_type = pl011_get8();
    switch (packet_type) {
    case HCI_EVENT: {
        struct hci_event_pkt *evt = kmalloc_notzero(sizeof(*evt));
        _receive_event(evt);
        cq_push(&module.event_rx_buffer, evt);
        break;
    }
    case HCI_ACL_DATA: {
        struct hci_acl_data_pkt *acl = kmalloc_notzero(sizeof(*acl));
        _receive_acl_data(acl);
        cq_push(&module.acl_rx_buffer, acl);
        break;
    }
    default:
        panic("Unexpected BT packet type: %x\n", packet_type);
    }
}


/*
 * HCI Command:
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |           OpCode              |  Params len   |  Parameter 0  |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                           Parameter 1                         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                               ...                             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |           Parameter N-1       |           Parameter N         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
void bt_send_command(struct hci_command_pkt *cmd) {
    assert(module.initialized);

    // Wait until we are allowed to send a command. Ideally, here we would have
    // a timeout and reset the Bluetooth module if we wait too long.
    while (module.n_commands_can_send == 0) {
        _receive_packet();
    }
    module.n_commands_can_send--;

    // todo("Send HCI command packet. Be careful to send in little endian!");
    // first send the packet indicator byte
    // then send the opcode in little endian for 16 bit; params_len, params
    pl011_put8(HCI_CMD);
    pl011_put8(cmd->opcode & 0xFF);
    pl011_put8((cmd->opcode >> 8) & 0xFF);
    pl011_put8(cmd->params_len);
    for (int i = 0; i < cmd->params_len; i++) {
        pl011_put8(cmd->params[i]); // sending in little endian
    }
}

/*
 * HCI ACL Data:
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |         Handle        | Flags |       Data Total Length       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  |                              Data                             |
 *  |                               ...                             |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
void bt_send_acl_data(struct hci_acl_data_pkt *acl) {
    assert(module.initialized);
    // todo("Send HCI ACL data packet. Set the flags to 0.");
    // send indicator,
    // handle (little endian w upper 4 bits as flag=0)
    // data_len (little endian 16-bit),
    // data
    pl011_put8(HCI_ACL_DATA);
    pl011_put8(acl->handle & 0xFF);
    pl011_put8((acl->handle >> 8) & 0xFF); // upper 4 bits holds flags set to 0
    pl011_put8(acl->data_len & 0xFF);
    pl011_put8((acl->data_len >> 8) & 0xFF);
    for (int i = 0; i < acl->data_len; i++) {
        pl011_put8(acl->data[i]);
    }
}

extern unsigned char BCM43430A1_hcd[];
extern unsigned int BCM43430A1_hcd_len;

void bt_upload_firmware(void) {
    assert(module.initialized);

    // Send a reset and a load firmware command
    struct hci_command_pkt cmd = { 0 };
    cmd.opcode = CMD_RESET;
    bt_send_command(&cmd);
    cmd.opcode = CMD_BCM_LOAD_FIRMWARE;
    bt_send_command(&cmd);

    // Wait for command complete for the load firmware command
    while (1) {
        struct hci_event_pkt *event = bt_receive_event();
        if (event->event_code == EVENT_COMMAND_COMPLETE) {
            assert(event->params_len >= 3);
            u16 opcode = event->params[1] | (event->params[2] << 8);
            if (opcode == CMD_BCM_LOAD_FIRMWARE)
                break;
        }
    }

    printk("About to upload firmware...\n");

    delay_ms(50); // same time as linux

    int n_packets_sent = 0;
    // todo("Iterate over the firmware and send it in chunks. Keep track of the number of packets in n_packets_sent.");
    // we send a sequence of HCI commands: [opcode (2)][params_len (1)][params (N)]
    unsigned int index = 0;
    while (index < BCM43430A1_hcd_len) {
        struct hci_command_pkt command = {0};
        command.opcode = (u16)BCM43430A1_hcd[index] | ((u16)BCM43430A1_hcd[index + 1] << 8);
        index += 2; // increment index by 2!!
        command.params_len = BCM43430A1_hcd[index++];
        for (int i = 0; i < command.params_len; i++) {
            command.params[i] = BCM43430A1_hcd[index++];
        }
        bt_send_command(&command);
        n_packets_sent++;
    }

    assert(n_packets_sent == 121); // sanity check based on Javier's code

    // Purge event queue
    // todo("Purge all the command complete events, (hint: you know that "
    //         "you sent n_packets_sent commands)");
    // empty the event queue: 
    // drain the command-complete event generated by each firmware chunk
    for (int i = 0; i < n_packets_sent; i++) {
        bt_receive_event();
    }

    printk("Waiting 250ms for patch to take effect...\n");
    delay_ms(250); // same time as linux
}

// Functions for pulling from the receive buffers

struct hci_event_pkt *bt_receive_event(void) {
    assert(module.initialized);
    while (cq_empty(&module.event_rx_buffer)) {
        _receive_packet();
    }
    return cq_pop(&module.event_rx_buffer);
}

struct hci_event_pkt *bt_receive_event_async(void) {
    assert(module.initialized);
    if (cq_empty(&module.event_rx_buffer)) {
        if (pl011_has_data()) {
            _receive_packet();
            if (cq_empty(&module.event_rx_buffer)) {
                return NULL;
            }
        } else {
            return NULL;
        }
    }
    return cq_pop(&module.event_rx_buffer);
}

// Tell the local controller the host has consumed one ACL packet, freeing
// buffer space so the controller will forward the next one from the peer.
// Without this, the controller stalls after delivering max_packets (8) to us.
static void _host_ack_acl(u16 handle) {
    // HCI_Host_Number_Of_Completed_Packets: opcode 0x0035, no OGF.
    // params: num_handles(1), handle_lo, handle_hi, count_lo, count_hi
    // packet: indicator(1) + opcode(2) + params_len(1) + params(5) = 9 bytes
    // params: num_handles(1) + handle(2) + num_completed(2)
    u8 buf[9];
    buf[0] = HCI_CMD;
    buf[1] = CMD_HOST_NUM_COMPLETED_PACKETS & 0xFF;
    buf[2] = (CMD_HOST_NUM_COMPLETED_PACKETS >> 8) & 0xFF;
    buf[3] = 5;               // params_len
    buf[4] = 1;               // num_handles = 1
    buf[5] = handle & 0xFF;
    buf[6] = (handle >> 8) & 0xFF;
    buf[7] = 1;               // num_completed lo
    buf[8] = 0;               // num_completed hi
    // Note: this command generates no event response, so we send raw bytes.
    for (int i = 0; i < 9; i++)
        pl011_put8(buf[i]);
}

struct hci_acl_data_pkt *bt_receive_acl(void) {
    assert(module.initialized);
    while (cq_empty(&module.acl_rx_buffer)) {
        _receive_packet();
    }
    struct hci_acl_data_pkt *pkt = cq_pop(&module.acl_rx_buffer);
    _host_ack_acl(pkt->handle & 0x0FFF); // lower 12 bits = connection handle
    return pkt;
}

struct hci_acl_data_pkt *bt_receive_acl_async(void) {
    assert(module.initialized);
    if (cq_empty(&module.acl_rx_buffer)) {
        if (pl011_has_data()) {
            _receive_packet();
            if (cq_empty(&module.acl_rx_buffer)) {
                return NULL;
            }
        } else {
            return NULL;
        }
    }
    return cq_pop(&module.acl_rx_buffer);
}

// ===========================================================================
//  High-level connection API
// ===========================================================================

// Switch both the BCM chip and the Pi PL011 UART to a new baud rate.
// Must be called after bt_init() + bt_upload_firmware() and before any
// further HCI traffic. Valid rates: 115200 (no-op), 921600, 1500000, 3000000.
// The BCM chip acks at the OLD baud, then flips; we follow after a short delay.
void bt_set_baud(unsigned baud) {
    if (baud == 115200)
        return;

    printk("Switching UART baud to %d...\n", baud);

    struct hci_command_pkt cmd = {0};
    cmd.opcode = CMD_BCM_UPDATE_BAUDRATE;
    cmd.params_len = 6;
    cmd.params[0] = 0x00; // two reserved bytes
    cmd.params[1] = 0x00;
    cmd.params[2] = baud & 0xff;         // baud, little-endian
    cmd.params[3] = (baud >> 8) & 0xff;
    cmd.params[4] = (baud >> 16) & 0xff;
    cmd.params[5] = (baud >> 24) & 0xff;
    bt_send_command(&cmd);

    // ACK arrives at the old baud rate; chip flips immediately after sending it.
    struct hci_event_pkt *evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0);

    delay_ms(10);
    pl011_set_baud(baud); // now move the Pi side to match
    delay_ms(10);
    printk("Baud switched to %d.\n", baud);
}

// Full chip bringup: init, firmware, optional baud switch, read+print BD_ADDR,
// set local name, read buffer sizes. Call once before bt_connect_*.
void bt_setup(const char *local_name, unsigned baud) {
    bt_init();
    bt_upload_firmware();
    bt_set_baud(baud);

    struct hci_command_pkt cmd = {0};
    struct hci_event_pkt *evt;

    cmd.opcode = CMD_READ_BD_ADDR;
    bt_send_command(&cmd);
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 10);
    assert(evt->params[3] == 0);
    printk("BD_ADDR: ");
    for (int i = 5; i >= 0; i--) {
        u8 b = evt->params[4 + i];
        printk("%X%X", b >> 4, b & 0xf);
        if (i != 0) printk(":");
    }
    printk("\n");

    cmd.opcode = CMD_WRITE_LOCAL_NAME;
    cmd.params_len = 248;
    memset(cmd.params, 0, sizeof(cmd.params));
    // local_name length bounded by params array; truncated silently if > 247
    for (int i = 0; i < 247 && local_name[i]; i++)
        cmd.params[i] = local_name[i];
    bt_send_command(&cmd);
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params[3] == 0);

    cmd.opcode = CMD_READ_BUFFER_SIZE;
    cmd.params_len = 0;
    bt_send_command(&cmd);
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params[3] == 0);
    printk("Max ACL packet len: %d, max packets: %d\n",
        evt->params[4] | (evt->params[5] << 8),
        evt->params[7] | (evt->params[8] << 8));
}

// Dial peer_addr_be (big-endian). page_timeout units are 0.625ms each;
// 0xFFFF (~40.9s) is a safe choice when the peer may be slow to appear.
// Returns the ACL connection handle on success; asserts on failure.
u16 bt_connect_initiator(const u8 peer_addr_be[6], u16 page_timeout) {
    u8 peer_le[6];
    for (int i = 0; i < 6; i++)
        peer_le[i] = peer_addr_be[5 - i];

    struct hci_command_pkt cmd = {0};
    struct hci_event_pkt *evt;

    cmd.opcode = CMD_WRITE_PAGE_TIMEOUT;
    cmd.params_len = 2;
    cmd.params[0] = page_timeout & 0xff;
    cmd.params[1] = (page_timeout >> 8) & 0xff;
    bt_send_command(&cmd);
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params[3] == 0);

    u16 handle;
    while (1) {
        cmd.opcode = CMD_CREATE_CONNECTION;
        cmd.params_len = 13;
        memcpy(&cmd.params[0], peer_le, 6);
        cmd.params[6]  = 0x18; // packet types 0xcc18 = allow all classic
        cmd.params[7]  = 0xcc;
        cmd.params[8]  = 0x02; // page scan repetition mode R2
        cmd.params[9]  = 0x00; // reserved
        cmd.params[10] = 0x00; // clock offset (none)
        cmd.params[11] = 0x00;
        cmd.params[12] = 0x00; // no role switch
        bt_send_command(&cmd);

        evt = bt_receive_event();
        assert(evt->event_code == EVENT_COMMAND_STATUS);
        assert(evt->params[0] == 0); // pending OK

        evt = bt_receive_event();
        assert(evt->event_code == EVENT_CONNECTION_COMPLETE);
        u8 status = evt->params[0];
        if (status == 0x04) {
            printk("page timeout, retrying...\n");
            continue;
        }
        assert(status == 0); // status success
        assert(evt->params[9] == 1); // ACL link

        handle = evt->params[1] | ((u16)evt->params[2] << 8);
        break;
    }

    printk("Connected (initiator)! Handle: %X\n", handle);
    return handle;
}

// Enable page scanning and wait for peer_addr_be to call in.
// Returns the ACL connection handle on success; asserts on failure.
u16 bt_connect_acceptor(const u8 peer_addr_be[6]) {
    u8 peer_le[6];
    for (int i = 0; i < 6; i++)
        peer_le[i] = peer_addr_be[5 - i];

    struct hci_command_pkt cmd = {0};
    struct hci_event_pkt *evt;

    cmd.opcode = CMD_WRITE_SCAN_ENABLE;
    cmd.params_len = 1;
    cmd.params[0] = 0x02; // page scan only
    bt_send_command(&cmd);
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params[3] == 0);

    printk("Waiting for connection from peer...\n");

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_CONNECTION_REQUEST);
    assert(memcmp(&evt->params[0], peer_le, 6) == 0);
    assert(evt->params[9] == 1); // ACL link

    cmd.opcode = CMD_ACCEPT_CONNECTION_REQUEST;
    cmd.params_len = 7;
    memcpy(&cmd.params[0], peer_le, 6);
    cmd.params[6] = 0x01; // stay slave
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_STATUS);
    assert(evt->params[0] == 0); // pending OK

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_CONNECTION_COMPLETE);
    assert(evt->params[0] == 0); // status success
    assert(evt->params[9] == 1); // ACL link

    u16 handle = evt->params[1] | ((u16)evt->params[2] << 8);
    printk("Connected (acceptor)! Handle: %X\n", handle);
    return handle;
}

// Send one ACL data packet on the given connection handle.
void bt_send_packet(u16 handle, const u8 *data, u16 len) {
    struct hci_acl_data_pkt acl = {0};
    acl.handle = handle;
    if (len > sizeof(acl.data))
        len = sizeof(acl.data);
    acl.data_len = len;
    memcpy(acl.data, data, len);
    bt_send_acl_data(&acl);
}

// Send a packed-RGB framebuffer (0x00RRGGBB per pixel) as raw byte triplets.
// Splits into ACL packets internally; no higher-level framing is added.
void bt_send_image(u16 handle, const u32 *rgb, u32 width, u32 height) {
    // 1020 = largest multiple of 3 that fits in one ACL payload (max 1021),
    // so no RGB triplet is ever split across a packet boundary.
    #define BT_IMAGE_CHUNK 1020
    #define BT_MAX_IN_FLIGHT 4

    u8 chunk[BT_IMAGE_CHUNK];
    u16 pos = 0;
    u32 npixels = width * height;
    int in_flight = 0;

    // Drain any NUMBER_OF_COMPLETED_PACKETS events, returning credits.
    // Uses async (non-blocking) receive so we only drain what's arrived.
    #define drain_completed() do { \
        struct hci_event_pkt *_e; \
        while ((_e = bt_receive_event_async()) != NULL) { \
            if (_e->event_code == EVENT_NUMBER_OF_COMPLETED_PACKETS) { \
                u8 _nh = _e->params[0]; \
                for (int _i = 0; _i < _nh; _i++) \
                    in_flight -= (_e->params[3 + _i*4] | (_e->params[4 + _i*4] << 8)); \
            } \
        } \
    } while (0)

    for (u32 i = 0; i < npixels; i++) {
        chunk[pos++] = (rgb[i] >> 16) & 0xFF; // R
        chunk[pos++] = (rgb[i] >>  8) & 0xFF; // G
        chunk[pos++] = (rgb[i]      ) & 0xFF; // B

        if (pos == BT_IMAGE_CHUNK) {
            // Wait until the controller has buffer space before sending.
            while (in_flight >= BT_MAX_IN_FLIGHT) {
                struct hci_event_pkt *e = bt_receive_event();
                if (e->event_code == EVENT_NUMBER_OF_COMPLETED_PACKETS) {
                    u8 nh = e->params[0];
                    for (int j = 0; j < nh; j++)
                        in_flight -= (e->params[3 + j*4] | (e->params[4 + j*4] << 8));
                }
            }
            bt_send_packet(handle, chunk, pos);
            in_flight++;
            pos = 0;
            drain_completed();
        }
    }
    if (pos > 0) {
        while (in_flight >= BT_MAX_IN_FLIGHT) {
            struct hci_event_pkt *e = bt_receive_event();
            if (e->event_code == EVENT_NUMBER_OF_COMPLETED_PACKETS) {
                u8 nh = e->params[0];
                for (int j = 0; j < nh; j++)
                    in_flight -= (e->params[3 + j*4] | (e->params[4 + j*4] << 8));
            }
        }
        bt_send_packet(handle, chunk, pos);
    }
}

// Send an opaque byte buffer over BT, chunked into ACL packets with the same
// credit-based flow control as bt_send_image. Used for compressed payloads.
// Returns 1 if the whole buffer was sent, 0 if it gave up (peer stopped
// returning buffer credits for BT_SEND_TIMEOUT_US -- e.g. the link dropped),
// so the caller isn't stuck forever waiting on a dead peer.
int bt_send_raw(u16 handle, const u8 *data, u32 len) {
    #define BT_RAW_CHUNK 1020
    #define BT_RAW_IN_FLIGHT 4
    #define BT_SEND_TIMEOUT_US 5000000u   // 5s of NO progress -> abort (link dead).
                                          // reset on every credit, so a slow-but-
                                          // alive link still completes the send
                                          // (a full frame can take 15-30s).
    u32 sent = 0;
    int in_flight = 0;

    #define drain_completed_raw() do { \
        struct hci_event_pkt *_e; \
        while ((_e = bt_receive_event_async()) != NULL) { \
            if (_e->event_code == EVENT_NUMBER_OF_COMPLETED_PACKETS) { \
                u8 _nh = _e->params[0]; \
                for (int _i = 0; _i < _nh; _i++) \
                    in_flight -= (_e->params[3 + _i*4] | (_e->params[4 + _i*4] << 8)); \
            } \
        } \
    } while (0)

    // Flush stale completed-packets events left over from the previous frame.
    // If we let drain_completed_raw() count them, in_flight goes negative, the
    // "in_flight >= MAX" flow-control gate stops firing, and we blast the whole
    // frame without waiting -- overrunning the controller so it stops returning
    // credits. That drift compounds and the link dies after ~2 frames. Discard
    // them here (without touching in_flight) so each send starts clean.
    { struct hci_event_pkt *_stale;
      while ((_stale = bt_receive_event_async()) != NULL) (void)_stale; }

    while (sent < len) {
        u16 n = (len - sent) > BT_RAW_CHUNK ? BT_RAW_CHUNK : (u16)(len - sent);
        // wait for the controller to free a buffer credit. reset the clock on
        // every credit so only a genuine stall (5s of silence) aborts.
        u32 t0 = timer_get_usec();
        while (in_flight >= BT_RAW_IN_FLIGHT) {
            struct hci_event_pkt *e = bt_receive_event_async();
            if (e) {
                if (e->event_code == EVENT_NUMBER_OF_COMPLETED_PACKETS) {
                    u8 nh = e->params[0];
                    for (int j = 0; j < nh; j++)
                        in_flight -= (e->params[3 + j*4] | (e->params[4 + j*4] << 8));
                }
                t0 = timer_get_usec();   // made progress; reset the clock
            } else if (timer_get_usec() - t0 > BT_SEND_TIMEOUT_US) {
                return 0;                // peer not acking -> give up
            }
        }
        bt_send_packet(handle, data + sent, n);
        in_flight++;
        sent += n;
        drain_completed_raw();
    }
    return 1;
}
