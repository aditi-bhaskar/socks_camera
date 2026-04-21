# photobooth display: read frames off the serial port (christy's pi), show them
# in a grid, and save every one to captured/.
#
# setup: python3 -m venv venv && venv/bin/pip install -r requirements.txt
# run:   venv/bin/python laptop-side-display.py [serial-port]

import os
import re
import sys
import serial
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

# paths / config
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSETS_DIR = os.path.join(SCRIPT_DIR, "assets")
SAVE_DIR = os.path.join(SCRIPT_DIR, "captured")
# raspberry-themed border; falls back to a generated berry pattern if missing.
BG_PATH = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "include", "raspberries.png"))
os.makedirs(SAVE_DIR, exist_ok=True)
os.makedirs(ASSETS_DIR, exist_ok=True)

WIN = "CS240lx Photobooth"

# grid layout. cols/rows are set at runtime by the trackbars; the canvas size is
# derived from them (see canvas_size).
MT = 30            # top margin
SM = 30            # side margin
GAP = 20           # gap between cards
BANNER_H = 110     # bottom area for the title
SH = 300           # card height for multi-photo grids
CW = 380           # card width for multi-photo grids
NATIVE_W = 640     # native camera res, used to size the 1x1 card at full res
NATIVE_H = 480
PAD = 10           # padding between card edge and photo
RADIUS = 16        # card corner radius

# grid bounds + default
COLS_MIN, COLS_MAX = 1, 4
ROWS_MIN, ROWS_MAX = 1, 4
COLS_DEFAULT, ROWS_DEFAULT = 1, 3


def card_size(cols, rows):
    """(cw, sh) for one card. 1x1 uses native camera resolution."""
    if cols == 1 and rows == 1:
        return NATIVE_W + 2 * PAD, NATIVE_H + 2 * PAD
    return CW, SH


def canvas_size(cols, rows):
    cw, sh = card_size(cols, rows)
    w = SM * 2 + cols * cw + (cols - 1) * GAP
    h = MT + rows * sh + (rows - 1) * GAP + BANNER_H
    return w, h

# colors (RGB, the canvas is built in PIL)
RASPBERRY = (190, 28, 66)
LEAF_GREEN = (120, 158, 96)
CARD_FILL = (255, 255, 255)
EMPTY_FILL = (248, 240, 243)

# cursive fonts to try, in order of preference
FONT_CANDIDATES = [
    ("/System/Library/Fonts/Supplemental/SnellRoundhand.ttc", 0),
    ("/System/Library/Fonts/Supplemental/Savoye LET.ttc", 0),
    ("/System/Library/Fonts/Supplemental/Apple Chancery.ttf", 0),
    ("/System/Library/Fonts/Supplemental/Brush Script.ttf", 0),
    ("/System/Library/Fonts/Supplemental/Zapfino.ttf", 0),
]
TITLE = "CS240lx 2026"

# serial / wire format (must match main_aditi.c)
MAGIC = 0x42424242
FMT_RGB888 = 0   # payload = w*h*3 raw R,G,B bytes
FMT_RGB565 = 1   # payload = w*h*2 big-endian 5:6:5 per pixel
# override the port with a CLI arg or the SERIAL_PORT env var.
DEFAULT_PORT = '/dev/tty.usbserial-310'
def pick_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    if os.environ.get("SERIAL_PORT"):
        return os.environ["SERIAL_PORT"]
    return DEFAULT_PORT

PORT = pick_port()
ser = serial.Serial(
    port=PORT,
    baudrate=2840909,
    bytesize=serial.EIGHTBITS,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    timeout=40
)
print(f"port {PORT} opened, waiting for data...")


def read_exact(n):
    buf = b''
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        buf += chunk
    return buf


def read_u32():
    return int.from_bytes(read_exact(4), 'big')


def find_magic():
    print("hunting for magic...")
    window = 0
    line = bytearray()   # christy's text between frames
    while True:
        data = ser.read(1)
        if len(data) == 0:
            continue  # timeout, keep waiting
        b = data[0]
        # echo christy's [perf] lines (printed between frames) so they're visible.
        # frame payload bytes never reach here -- read_frame reads them directly.
        if b == 0x0A:                       # newline -> flush the line
            if line:
                print("  [christy]", line.decode("ascii", "replace").rstrip())
                line = bytearray()
        elif 32 <= b <= 126:                # printable -> accumulate
            line.append(b)
            if len(line) > 256:
                line = bytearray()
        else:                               # non-text byte -> not a status line
            line = bytearray()
        window = ((window << 8) | b) & 0xFFFFFFFF
        if window == MAGIC:
            print("found magic!")
            return


def read_frame():
    find_magic()
    w = read_u32()
    h = read_u32()
    fmt = read_u32()
    payload_len = read_u32()
    print(f"reading frame {w}x{h} fmt={fmt} payload={payload_len}B...")
    payload = read_exact(payload_len)

    if fmt == FMT_RGB888:
        arr = np.frombuffer(payload, dtype=np.uint8).reshape(h, w, 3)
        return arr[:, :, ::-1]  # RGB -> BGR for OpenCV / saving
    elif fmt == FMT_RGB565:
        # big-endian 5:6:5 -> expand each channel back to 8 bits
        v = np.frombuffer(payload, dtype='>u2').reshape(h, w).astype(np.uint16)
        r = ((v >> 11) & 0x1F) << 3
        g = ((v >> 5) & 0x3F) << 2
        b = (v & 0x1F) << 3
        bgr = np.dstack([b, g, r]).astype(np.uint8)  # BGR for OpenCV / saving
        return bgr
    else:
        raise ValueError(f"unknown frame format {fmt}")


# image saving: numbered files that keep counting up across restarts.
def next_save_index():
    biggest = 0
    for name in os.listdir(SAVE_DIR):
        m = re.match(r"^(\d+)\.png$", name)
        if m:
            biggest = max(biggest, int(m.group(1)))
    return biggest + 1


_save_counter = next_save_index()


def save_frame(frame_bgr):
    global _save_counter
    path = os.path.join(SAVE_DIR, f"{_save_counter:04d}.png")
    cv2.imwrite(path, frame_bgr)  # already BGR
    print(f"saved {path}")
    _save_counter += 1


# photobooth rendering
def _load_font(size):
    for path, idx in FONT_CANDIDATES:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size, index=idx)
            except Exception:
                continue
    return ImageFont.load_default()


def _make_fallback_background(w, h):
    """Scattered-berry pattern, used when the border PNG is absent."""
    img = Image.new("RGB", (w, h), (255, 255, 255))
    draw = ImageDraw.Draw(img)
    rng = np.random.default_rng(240)  # deterministic so it doesn't flicker
    for _ in range(90):
        cx, cy = rng.integers(0, w), rng.integers(0, h)
        r = int(rng.integers(8, 22))
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=RASPBERRY,
                     outline=(120, 18, 44), width=2)
        # little leaf
        draw.polygon([(cx, cy - r), (cx - 6, cy - r - 8), (cx + 6, cy - r - 8)],
                     fill=LEAF_GREEN)
    return img


# cache the background per canvas size (size changes when the grid trackbars move).
_bg_cache = {}
_bg_src = None  # full-res border image, loaded once


def get_background(w, h):
    global _bg_src
    cached = _bg_cache.get((w, h))
    if cached is not None:
        return cached
    if os.path.exists(BG_PATH):
        if _bg_src is None:
            _bg_src = Image.open(BG_PATH).convert("RGB")
            print(f"using border background: {BG_PATH}")
        bg = _bg_src.resize((w, h))
    else:
        bg = _make_fallback_background(w, h)
        print(f"(no {os.path.basename(BG_PATH)} found — using generated berry pattern; "
              f"drop your PNG in {ASSETS_DIR} for the real one)")
    _bg_cache[(w, h)] = bg
    return bg


def _fit_into(frame_rgb, box_w, box_h):
    """Resize keeping aspect ratio, letterboxed onto a white box_w x box_h box."""
    h, w = frame_rgb.shape[:2]
    scale = min(box_w / w, box_h / h)
    nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
    resized = cv2.resize(frame_rgb, (nw, nh), interpolation=cv2.INTER_AREA)
    canvas = np.full((box_h, box_w, 3), 255, dtype=np.uint8)
    y0 = (box_h - nh) // 2
    x0 = (box_w - nw) // 2
    canvas[y0:y0 + nh, x0:x0 + nw] = resized
    return Image.fromarray(canvas)


def render(recent, cols, rows):
    """recent: BGR frames, newest last. fills the grid in reading order; a
    partial set leaves empty cards at the top-left."""
    canvas_w, canvas_h = canvas_size(cols, rows)
    canvas = get_background(canvas_w, canvas_h).copy()
    draw = ImageDraw.Draw(canvas)

    cw, sh = card_size(cols, rows)
    photo_w, photo_h = cw - 2 * PAD, sh - 2 * PAD
    capacity = cols * rows

    # keep the newest `capacity` frames, padded with None so a partial set fills
    # from the bottom-right up.
    shown = list(recent[-capacity:])
    slots = [None] * (capacity - len(shown)) + shown

    for i, frame in enumerate(slots):
        r, c = divmod(i, cols)
        x_card = SM + c * (cw + GAP)
        y_card = MT + r * (sh + GAP)
        if frame is None:
            draw.rounded_rectangle(
                [x_card, y_card, x_card + cw, y_card + sh],
                radius=RADIUS, fill=EMPTY_FILL, outline=RASPBERRY, width=3)
            continue
        # white card behind the photo
        draw.rounded_rectangle(
            [x_card, y_card, x_card + cw, y_card + sh],
            radius=RADIUS, fill=CARD_FILL, outline=RASPBERRY, width=3)
        photo = _fit_into(frame[:, :, ::-1], photo_w, photo_h)  # BGR -> RGB
        canvas.paste(photo, (x_card + PAD, y_card + PAD))

    # title in the bottom banner, shrunk to fit the width
    banner_top = MT + rows * SH + (rows - 1) * GAP
    size = 80
    font = _load_font(size)
    while size > 20:
        bbox = draw.textbbox((0, 0), TITLE, font=font)
        if bbox[2] - bbox[0] <= canvas_w - 40:
            break
        size -= 4
        font = _load_font(size)
    bbox = draw.textbbox((0, 0), TITLE, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (canvas_w - tw) // 2 - bbox[0]
    ty = banner_top + (BANNER_H - th) // 2 - bbox[1]
    draw.text((tx, ty), TITLE, font=font, fill=RASPBERRY,
              stroke_width=4, stroke_fill=(255, 255, 255))

    return cv2.cvtColor(np.array(canvas), cv2.COLOR_RGB2BGR)


# main loop
import threading

# serial reads block (up to the timeout), so read on a background thread and hand
# frames to the UI thread. keeps the trackbars responsive between photos.
_recent_lock = threading.Lock()
recent = []          # newest last
_new_frame = threading.Event()
_stop = threading.Event()


def reader_loop():
    while not _stop.is_set():
        try:
            frame = read_frame()
        except Exception as e:
            if _stop.is_set():
                return
            print(f"reader error: {e}")
            continue
        print("got frame, displaying...")
        # save before rendering so a display error can't skip a save.
        save_frame(frame)
        with _recent_lock:
            recent.append(frame)
            del recent[:-(COLS_MAX * ROWS_MAX)]   # cap history to the largest grid
        _new_frame.set()


reader = threading.Thread(target=reader_loop, daemon=True)
reader.start()

cv2.namedWindow(WIN, cv2.WINDOW_NORMAL)


# grid-size trackbars. they're integer-only but can slide to 0, so the callbacks
# snap any out-of-range value back to the nearest valid one.
def _snap(name, lo, hi):
    def cb(v):
        clamped = min(hi, max(lo, v))
        if clamped != v:
            cv2.setTrackbarPos(name, WIN, clamped)
    return cb


cv2.createTrackbar("cols", WIN, COLS_DEFAULT, COLS_MAX, _snap("cols", COLS_MIN, COLS_MAX))
cv2.createTrackbar("rows", WIN, ROWS_DEFAULT, ROWS_MAX, _snap("rows", ROWS_MIN, ROWS_MAX))

# hard minimum where supported (OpenCV 3.x+); harmless if missing.
try:
    cv2.setTrackbarMin("cols", WIN, COLS_MIN)
    cv2.setTrackbarMin("rows", WIN, ROWS_MIN)
except AttributeError:
    pass


def grid_from_trackbars():
    cols = min(COLS_MAX, max(COLS_MIN, cv2.getTrackbarPos("cols", WIN)))
    rows = min(ROWS_MAX, max(ROWS_MIN, cv2.getTrackbarPos("rows", WIN)))
    return cols, rows


window_open = True
last_grid = None

try:
    while True:
        cols, rows = grid_from_trackbars()
        grid_changed = (cols, rows) != last_grid

        # redraw on a new frame, a grid change, or the first pass.
        if _new_frame.is_set() or grid_changed or last_grid is None:
            _new_frame.clear()
            with _recent_lock:
                snapshot = list(recent)
            if window_open:
                if grid_changed:
                    cv2.resizeWindow(WIN, *canvas_size(cols, rows))
                cv2.imshow(WIN, render(snapshot, cols, rows))
            last_grid = (cols, rows)

        if window_open:
            if (cv2.waitKey(30) & 0xFF) == ord('q'):
                break
            # window's X clicked: stop showing, but keep capturing + saving.
            if cv2.getWindowProperty(WIN, cv2.WND_PROP_VISIBLE) < 1:
                window_open = False
                cv2.destroyWindow(WIN)
                print("window closed — still capturing & saving to "
                      f"{SAVE_DIR}. Press Ctrl-C to stop.")
        else:
            _new_frame.wait(timeout=1.0)   # no window to pump; idle until next save
finally:
    _stop.set()
    cv2.destroyAllWindows()
    ser.close()
