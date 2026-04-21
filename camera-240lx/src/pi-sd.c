#include "fat_compat.h"
#include "rpi.h"
#include "pi-sd.h"

// SD block layer: rpi_os's emmc.h provides sd_init / sd_readblock /
// sd_writeblock / SD_OK (backed by emmc.c in librpi_os.a). we drop the 140e
// checksum tracing helpers (fast-hash32 / crc).
#include "emmc.h"

static int trace_p = 0;
static int init_p = 0;

int pi_sd_trace(int on_p) {
    int old = on_p;
    trace_p = on_p;
    return old;
}


// not sure if we should allow it to be called multiple times?
// just make it so the routines take in a handle.
int pi_sd_init(void) {
    if(sd_init() != SD_OK)
        panic("sd_init failed\n");
    init_p = 1;
    return 1;
}

// Like pi_sd_init() but returns 0 on failure instead of panicking, so callers
// can gracefully disable SD features when no card is present.
int pi_sd_init_try(void) {
    if(sd_init() != SD_OK)
        return 0;
    init_p = 1;
    return 1;
}

int pi_sd_read(void *data, uint32_t lba, uint32_t nsec) {
  demand(init_p, "SD card not initialized!\n");
  int res;
  if((res = sd_readblock(lba, data, nsec)) != 512 * nsec)
    panic("could not read from sd card: result = %d\n", res);

  if(trace_p)
    trace("sd_read: lba=<%x>, nsec=%d\n", lba, nsec);
  return 1;
}

// allocate <nsec> worth of space, read in from SD card, return pointer.
// your kmalloc better work!
void *pi_sec_read(uint32_t lba, uint32_t nsec) {
  demand(init_p, "SD card not initialized!\n");
   // output("about to allocate %d\n", nsec * 512);
  uint8_t *data = kmalloc(nsec * 512);
  if(!pi_sd_read(data, lba, nsec))
    panic("could not read from sd card\n");
  return data;
}
#if 0
#endif

int pi_sd_write(void *data, uint32_t lba, uint32_t nsec) {
  demand(init_p, "SD card not initialized!\n");
  int res;
  if((res = sd_writeblock(data, lba, nsec)) != 512 * nsec)
    panic("could not write to sd card: result = %d\n", res);

  if(trace_p)
    trace("sd_write: lba=<%x>, nsec=%d\n", lba, nsec);
  return 1;
}
