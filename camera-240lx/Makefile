# ===========================================================
# BUILD TARGET — change this to switch what you're building
# ===========================================================
#   aditi    — camera + OLED live view + BT image sender   (rpi_os build)
#   christy  — BT receiver -> laptop UART bridge            (libpi build)
#   both     — build BOTH kernel_aditi.img + kernel_christy.img in one go
#
# Output:
#   aditi   -> kernel_aditi.img
#   christy -> kernel_christy.img
# other modes: sd-test, live
BUILD = both 
# ===========================================================

# strip any stray whitespace so "aditi " still matches "aditi"
BUILD := $(strip $(BUILD))

ifeq ($(BUILD),both)
# ===========================================================
# both: recurse into the two real builds (no flashing) and gather both images.
# ===========================================================
.PHONY: all both
all both:
	$(MAKE) BUILD=aditi   aditi-img
	$(MAKE) BUILD=christy christy-img
	@echo "==> built kernel_aditi.img + kernel_christy.img"

else ifeq ($(BUILD),christy)
# ===========================================================
# Christy: BT receiver, libpi build (no camera/OLED)
# ===========================================================
PROGS = src/main_christy.c

COMMON_SRC += src/bt.c
COMMON_SRC += src/BCM43430A1.c
COMMON_SRC += src/pl011.c

INC = -Iinclude -Irpi_os/include

OPT_LEVEL    = -Og
LIB_POST    += --no-warn-mismatch
BOOTLOADER   = my-install
RUN          = 1

include $(CS240LX_2026_PROJ_PATH)/libpi/mk/Makefile.robust

# Build the christy .bin without flashing and copy it to ./kernel_christy.img.
# gen-binaries (from Makefile.robust) builds the bins without invoking the
# bootloader/run step.
.PHONY: christy-img
christy-img: gen-binaries
	cp src/main_christy.bin kernel_christy.img
	@echo "==> kernel_christy.img"

else
# ===========================================================
# rpi_os builds: "aditi" (with BT) and "camera" (without)
# ===========================================================
CC      = arm-none-eabi-gcc
LD      = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy

RPI_OS_DIR    = rpi_os
RPI_OS_INC    = $(RPI_OS_DIR)/include
RPI_OS_LIB    = $(RPI_OS_DIR)/build/librpi_os.a
LINKER_SCRIPT = $(RPI_OS_DIR)/linker.ld

SRC_DIR   = src
BUILD_DIR = build

BASE_CFLAGS = -mcpu=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -fpic -ffreestanding \
              -O2 -Wall -Wextra -nostdlib

# Ported 140e FAT32 stack + SD-save helpers (aditi-only; kept out of plain camera).
FAT_SRCS = $(SRC_DIR)/fat32.c $(SRC_DIR)/fat32-helpers.c \
           $(SRC_DIR)/fat32-lfn-helpers.c $(SRC_DIR)/mbr.c \
           $(SRC_DIR)/mbr-helpers.c $(SRC_DIR)/pi-sd.c \
           $(SRC_DIR)/unicode-utf8.c $(SRC_DIR)/fat_heap.c $(SRC_DIR)/qoi_enc.c \
           $(SRC_DIR)/fat_libc.c

# Pure camera/display sources: everything in src/ EXCEPT mains, the BT stack,
# the FAT stack, and one-off test files. Each target adds back what it needs.
NON_CAMERA = $(SRC_DIR)/bt.c $(SRC_DIR)/BCM43430A1.c $(SRC_DIR)/pl011.c \
             $(SRC_DIR)/pl011_os.c $(SRC_DIR)/button.c \
             $(SRC_DIR)/frame_data.c \
             $(FAT_SRCS)
SRCS_QASM  = $(wildcard $(SRC_DIR)/*.qasm)
CAMERA_SRCS = $(filter-out $(NON_CAMERA) $(SRC_DIR)/main%.c, \
                $(wildcard $(SRC_DIR)/*.c) $(SRCS_QASM:.qasm=.c))

ifeq ($(BUILD),aditi)
# camera/display + BT stack (polling pl011_os) + buttons + FAT32 SD save + main
SRCS_C  = $(CAMERA_SRCS) \
          $(SRC_DIR)/button.c $(SRC_DIR)/bt.c $(SRC_DIR)/BCM43430A1.c \
          $(SRC_DIR)/pl011_os.c $(FAT_SRCS) $(SRC_DIR)/main_aditi.c
# -Iport FIRST so bt.c/pl011_os.c pick up the libpi compat shims (rpi.h,
# circular.h, gpio-high.h) instead of libpi / the raw-address gpio-high.
CFLAGS  = $(BASE_CFLAGS) -Iport -Iinclude -I$(RPI_OS_INC) -Irpi_os/bootloader/include
else ifeq ($(BUILD),sdtest)
# standalone SD/FAT32 write+readback test: just the FAT stack + its main.
SRCS_C  = $(FAT_SRCS) $(SRC_DIR)/main_sdtest.c
CFLAGS  = $(BASE_CFLAGS) -Iport -Iinclude -I$(RPI_OS_INC) -Irpi_os/bootloader/include
else ifeq ($(BUILD),live)
# livestream-only test: camera + BT (no SD, no buttons). uses the SAME wire
# format as a capture, so the normal christy + laptop display work unchanged.
SRCS_C  = $(CAMERA_SRCS) $(SRC_DIR)/bt.c $(SRC_DIR)/BCM43430A1.c \
          $(SRC_DIR)/pl011_os.c $(SRC_DIR)/main_live.c
CFLAGS  = $(BASE_CFLAGS) -Iport -Iinclude -I$(RPI_OS_INC) -Irpi_os/bootloader/include
else
# plain camera build
SRCS_C  = $(CAMERA_SRCS) $(SRC_DIR)/main.c
CFLAGS  = $(BASE_CFLAGS) -Iinclude -I$(RPI_OS_INC) -Irpi_os/bootloader/include
endif

ASFLAGS = -mcpu=arm1176jzf-s -mfpu=vfp
LDFLAGS = -T $(LINKER_SCRIPT) -nostdlib -mfloat-abi=hard -mfpu=vfp

SRCS_S = $(wildcard $(SRC_DIR)/*.S)
OBJS = \
  $(patsubst %.S,$(BUILD_DIR)/%.S.o,$(SRCS_S)) \
  $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(SRCS_C))

# Per-build image name: kernel_aditi.img / kernel_camera.img.
KERNEL_IMG = kernel_$(BUILD).img
# Per-build ELF too. A single shared elf name lets `make BUILD=aditi` after a
# sdtest/camera build see the elf as "up to date" and copy the WRONG binary
# into kernel_aditi.img. Naming it per-build forces a relink on every switch.
ELF = $(BUILD_DIR)/jankencamera_$(BUILD).elf

# build, then flash + run (pi-install over USB serial)
all: $(KERNEL_IMG)
	pi-install $(KERNEL_IMG)

$(RPI_OS_LIB):
	@if [ ! -f $(RPI_OS_LIB) ]; then $(MAKE) -C $(RPI_OS_DIR) lib; fi

$(KERNEL_IMG): $(ELF)
	$(OBJCOPY) $< -O binary $@

# build only, no flash
build-only: $(KERNEL_IMG)

# alias used by the "both" build (build, no flash)
.PHONY: aditi-img
aditi-img: $(KERNEL_IMG)
	@echo "==> $(KERNEL_IMG)"

$(ELF): $(RPI_OS_LIB) $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -L$(dir $(RPI_OS_LIB)) -lrpi_os -lgcc -o $@

$(BUILD_DIR)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
$(SRC_DIR)/%.c: $(SRC_DIR)/%.qasm
	/opt/homebrew/opt/vc4asm/bin/vc4asm -h $(SRC_DIR)/$*.h -c $(SRC_DIR)/$*.c $<

clean:
	rm -rf $(BUILD_DIR) kernel.img kernel_aditi.img kernel_camera.img kernel_christy.img
	$(MAKE) -C $(RPI_OS_DIR) clean

.PHONY: all clean

endif
