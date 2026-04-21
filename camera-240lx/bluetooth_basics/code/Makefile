# PROGS = 1-test-pl011.c
# PROGS = 2-hci-reset.c
# PROGS = 3-firmware-upload.c
# PROGS = 4-read-bdaddr.c
# PROGS = 5-connect.c
# PROGS = 5-accept-connection.c
PROGS = 6-aditi-custom.c


# # start
# BOOT:bootloader: Done.
# about to install interrupt handlers
# About to upload firmware...
# Waiting 250ms for patch to take effect...
# BD_ADDR: PANIC:./libc/printk.c:vprintk:99:bogus identifier: <0>

COMMON_SRC += bt.c
COMMON_SRC += BCM43430A1.c

# Replace with your own pl011.c!
COMMON_SRC += pl011.c
# STAFF_OBJS += staff-pl011.o

# if you need to override the install
BOOTLOADER = my-install

OPT_LEVEL = -Og

# staff-kmalloc.o was precompiled without VFP register args; suppress the
# ABI mismatch error since kmalloc only uses integer/pointer types (safe).
LIB_POST += --no-warn-mismatch

# automatically run the code
RUN = 1

include $(CS240LX_2026_PROJ_PATH)/libpi/mk/Makefile.robust


# One practical tip: flash the accepter (Bob) first so it's sitting at Waiting for connection..., then flash the connector (Alice). Otherwise Alice pages a device that isn't listening yet.