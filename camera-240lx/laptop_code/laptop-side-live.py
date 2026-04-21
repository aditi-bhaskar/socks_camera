# live viewer for main_live. shows the feed in one window, never saves.
# main_live sends a full keyframe then tile-deltas (only the changed 8x8 tiles),
# so most frames are tiny. christy forwards each message unchanged.
#
# run: venv/bin/python laptop-side-live.py [serial-port]

import sys
import serial
import numpy as np
import cv2

MAGIC = 0x42424242
FMT_LIVE_KEY = 2     # payload = W*H*2 big-endian RGB565
FMT_LIVE_DELTA = 3   # payload = ntiles(2) + ntiles*(idx(2) + TILE*TILE*2)

LIVE_W, LIVE_H = 160, 120
TILE = 8
TILES_X = LIVE_W // TILE
TILES_Y = LIVE_H // TILE

DEFAULT_PORT = '/dev/tty.usbserial-310'
PORT = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
WIN = "CS240lx Live"
SCALE = 4   # upscale the small stream for viewing

ser = serial.Serial(
    port=PORT,
    baudrate=2840909,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    timeout=40,
)
print(f"port {PORT} opened, waiting for live frames...")

# live framebuffer (RGB): keyframes replace it, deltas patch tiles.
live = np.zeros((LIVE_H, LIVE_W, 3), dtype=np.uint8)


def read_exact(n):
    buf = b''
    while len(buf) < n:
        buf += ser.read(n - len(buf))
    return buf


def read_u16():
    return int.from_bytes(read_exact(2), 'big')


def read_u32():
    return int.from_bytes(read_exact(4), 'big')


def find_magic():
    window = 0
    while True:
        data = ser.read(1)
        if not data:
            continue
        window = ((window << 8) | data[0]) & 0xFFFFFFFF
        if window == MAGIC:
            return


# big-endian RGB565 bytes -> (h, w, 3) RGB uint8.
def decode_565(buf, w, h):
    v = np.frombuffer(buf, dtype='>u2').reshape(h, w).astype(np.uint16)
    r = ((v >> 11) & 0x1F) << 3
    g = ((v >> 5) & 0x3F) << 2
    b = (v & 0x1F) << 3
    return np.dstack([r, g, b]).astype(np.uint8)


# read one message and apply it to `live`. returns True if `live` changed.
def read_message():
    global live
    find_magic()
    w, h = read_u32(), read_u32()
    fmt, payload_len = read_u32(), read_u32()
    payload = read_exact(payload_len)

    if fmt == FMT_LIVE_KEY:
        live = decode_565(payload, w, h)
        return True
    elif fmt == FMT_LIVE_DELTA:
        ntiles = int.from_bytes(payload[0:2], 'big')
        off = 2
        tbytes = TILE * TILE * 2
        for _ in range(ntiles):
            idx = int.from_bytes(payload[off:off+2], 'big'); off += 2
            tile = decode_565(payload[off:off+tbytes], TILE, TILE); off += tbytes
            tr, tc = divmod(idx, TILES_X)
            if tr < TILES_Y and tc < TILES_X:
                live[tr*TILE:(tr+1)*TILE, tc*TILE:(tc+1)*TILE] = tile
        return True
    return False


cv2.namedWindow(WIN, cv2.WINDOW_NORMAL)
cv2.resizeWindow(WIN, LIVE_W * SCALE, LIVE_H * SCALE)
try:
    while True:
        try:
            read_message()
        except Exception as e:
            print(f"decode error ({e}), resyncing...")
            continue
        big = cv2.resize(live, (LIVE_W * SCALE, LIVE_H * SCALE),
                         interpolation=cv2.INTER_NEAREST)
        cv2.imshow(WIN, cv2.cvtColor(big, cv2.COLOR_RGB2BGR))
        if (cv2.waitKey(1) & 0xFF) == ord('q'):
            break
finally:
    cv2.destroyAllWindows()
    ser.close()
