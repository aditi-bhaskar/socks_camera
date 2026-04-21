// Javier Garcia Nieto <jgnieto@cs.stanford.edu>. CS340LX Fall 2025.

// ADITI FILE

#include "bt.h"
#include "demand.h"
#include "hci-consts.h"
#include "rpi.h"
#include <string.h>

// Pick your own local name!
static const char LOCAL_NAME[] = "RPI Alice";

// This is Alice's file!

// Bob's address in big endian format (note that you have to send in little endian!)

// ALICE / ADITI ADDRESS
// b8:27:eb:0b:88:64
// 0xb8, 0x27, 0xeb, 0x0b, 0x88, 0x64,

static u8 peer_addr_be[] = { 
    0xb8, 0x27, 0xeb, 0x30, 0xd1, 0x77, // Christy's addr
    // 0x1e, 0x70, 0x01, 0x14, 0x68, 0x05, // equivalent to 1e:70:01:14:68:05
};

void notmain(void) {
    bt_init();
    bt_upload_firmware();

    u8 peer_addr_le[6];
    for (int i = 0; i < 6; i++) {
        peer_addr_le[i] = peer_addr_be[5 - i];
    }

    struct hci_command_pkt cmd = {0};
    struct hci_event_pkt *evt;
    struct hci_acl_data_pkt *acl_recv;

    // Read BD_ADDR
    cmd.opcode = CMD_READ_BD_ADDR;
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params_len == 10); // opcode (2) + num commands (1) + status (1) + bdaddr (6)
    assert(evt->params[3] == 0); // status == 0 (success)

    printk("BD_ADDR: ");
    for (int i = 5; i >= 0; i--) {
        u8 b = evt->params[4 + i];
        printk("%X%X", b >> 4, b & 0xf); // printk has no %02x padding support
        if (i != 0) {
            printk(":");
        }
    }
    printk("\n");

    // Write local name (for fun!)
    cmd.opcode = CMD_WRITE_LOCAL_NAME;
    cmd.params_len = 248;
    memcpy(cmd.params, LOCAL_NAME, sizeof(LOCAL_NAME));
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0); // status success

    // Check that local name was written
    cmd.opcode = CMD_READ_LOCAL_NAME;
    cmd.params_len = 0;
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 252);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0); // status success
    printk("Local name: %s\n", &evt->params[4]);

    // Read buffer size
    cmd.opcode = CMD_READ_BUFFER_SIZE;
    cmd.params_len = 0;
    bt_send_command(&cmd);

    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_COMPLETE);
    assert(evt->params_len == 11);
    assert(memcmp(&evt->params[1], &cmd.opcode, 2) == 0);
    assert(evt->params[3] == 0); // status success

    printk("Max ACL data packet length: %d\n", 
        evt->params[4] | (evt->params[5] << 8));
    printk("Max number ACL data packets: %d\n", 
        evt->params[7] | (evt->params[8] << 8));

    
    // todo("Use HCI_Create_Connection to connect to the peer device.");
    // // Use 0xcc18 as the packet types (allow all).
    // // No clock offset and no role switch.
    // NOTES: HCI_Create_Connection 
    //   BD_ADDR (6) + Packet_Type (2) + PSR_Mode (1) + 
    //   Reserved (1) + Clock_Offset (2) + Allow_Role_Switch (1)
    cmd.opcode = CMD_CREATE_CONNECTION;
    cmd.params_len = 13;
    // addr 6 bytes (from 0-5)
    memcpy(&cmd.params[0], peer_addr_le, 6); // BD_ADDR is little-endian
    // packet type. (0xcc18 = allow all classic types)
    cmd.params[6]  = 0x18;  // LSB 
    cmd.params[7]  = 0xcc;  // MSB
    // page scan repetition mode. this is R2
    cmd.params[8]  = 0x02;  
    // reserved 1 byte
    cmd.params[9]  = 0x00;  // Reserved!
    // clock offset 2 bytes: no offset
    cmd.params[10] = 0x00;  // LSB
    cmd.params[11] = 0x00;  // MSB
    // role switch 1 byte
    cmd.params[12] = 0x00;  // don't allow role switch
    bt_send_command(&cmd);

    // Since this is non-blocking, we expect a command status event rather than
    // a command complete event
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_COMMAND_STATUS);
    assert(evt->params_len == 4);
    assert(memcmp(&evt->params[2], &cmd.opcode, 2) == 0);
    assert(evt->params[0] == 0); // status successfully pending

    // Expect connection complete even if connection fails
    u8 handle[2];
    evt = bt_receive_event();
    assert(evt->event_code == EVENT_CONNECTION_COMPLETE);
    // NOTES params: 
    //   [0]=status [1-2]=handle [3-8]=BD_ADDR 
    //   [9]=link_type [10]=encryption

    // 1. todo("Verify Connection Complete event parameters");   
    assert(evt->params_len == 11);
    assert(evt->params[0] == 0);  // confirm that status = success
    assert(evt->params[9] == 1);  // confirm that the link_type is ACL
    
    // 2. todo("Copy connection handle from event parameters");
    // connection handle, send lsb first
    handle[0] = evt->params[1];   // LSB
    handle[1] = evt->params[2];   // MSB

    printk("Connected! Handle: %X%X%X%X\n",
        handle[1] >> 4, handle[1] & 0xf, handle[0] >> 4, handle[0] & 0xf);

    while (1) {
        evt = bt_receive_event_async();
        if (evt) {
            // very frequent event, so don't print always
            if (evt->event_code != EVENT_NUMBER_OF_COMPLETED_PACKETS) {
                printk("Received event code %X\n", evt->event_code);
                if (evt->event_code == EVENT_DISCONNECTION_COMPLETE) {
                    printk("\nDisconnected\n");
                    break;
                }
            }
        }

        acl_recv = bt_receive_acl_async();
        if (acl_recv) {
            for (int i = 0; i < acl_recv->data_len; i++) {
                uart_put8(acl_recv->data[i]);
            }
        }
        
        if (uart_has_data()) {
            struct hci_acl_data_pkt acl_send = {0};
            memcpy(&acl_send.handle, handle, 2);
            for (int i = 0; uart_has_data() && i < 1021; i++) {
                acl_send.data[i] = uart_get8();
                acl_send.data_len++;
            }
            bt_send_acl_data(&acl_send);
        }
    }
}
