// Javier Garcia Nieto <jgnieto@cs.stanford.edu>. CS340LX Fall 2025.

#define CQE_T void*

#include "bt.h"
#include "hci-consts.h"
#include "rpi.h"
#include "pl011.h"
#include "circular.h"
#include "gpio-high.h"

#include <stdbool.h>

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

struct hci_acl_data_pkt *bt_receive_acl(void) {
    assert(module.initialized);
    while (cq_empty(&module.acl_rx_buffer)) {
        _receive_packet();
    }
    return cq_pop(&module.acl_rx_buffer);
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
