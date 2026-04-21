#!/usr/bin/env python3
"""
wb_tuner.py — offline white balance tuner for jankencamera

CAPTURE MODE (run once to grab a raw frame from UART):
    python3 wb_tuner.py --capture --port /dev/tty.usbserial-XXXX --out frame.raw

TUNE MODE (run anytime after capture):
    python3 wb_tuner.py --tune frame.raw

USAGE
-----
In main.c, call uart_send_raw_frame(rgb_buf, w, h) BEFORE apply_white_balance().
This script receives it, saves it, then lets you drag sliders to preview
different WB multipliers until you find values you like.

DEPENDENCIES
    pip install pyserial numpy pillow
    (tkinter is part of the Python stdlib but needs 'python3-tk' on Linux)
"""

import argparse
import struct
import sys
import os
import time
import numpy as np

FRAME_MAGIC = 0x42424242
RAW_MAGIC   = 0xCAFEF00D   # distinct magic so Pi can send both kinds

# ---------------------------------------------------------------------------
# UART capture
# ---------------------------------------------------------------------------

def read_word(ser):
    return struct.unpack(">I", ser.read(4))[0]

def capture_raw_frame(port, baud, out_path, timeout=30):
    import serial
    print(f"[capture] opening {port} at {baud} baud, waiting for RAW_MAGIC 0x{RAW_MAGIC:08X}...")
    ser = serial.Serial(port, baud, timeout=1)

    deadline = time.time() + timeout
    magic = 0
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        magic = ((magic << 8) | b[0]) & 0xFFFFFFFF
        if magic == RAW_MAGIC:
            print("[capture] magic found!")
            break
    else:
        print("[capture] timed out waiting for magic")
        ser.close()
        sys.exit(1)

    w = read_word(ser)
    h = read_word(ser)
    print(f"[capture] frame size: {w}x{h}, reading {w*h*3} bytes...")

    data = b""
    total = w * h * 3
    while len(data) < total:
        chunk = ser.read(min(4096, total - len(data)))
        if not chunk:
            print(f"[capture] stalled at {len(data)}/{total} bytes")
            break
        data += chunk
        pct = 100 * len(data) / total
        print(f"\r[capture] {pct:.1f}%  ", end="", flush=True)

    print()
    ser.close()

    if len(data) < total:
        print(f"[capture] incomplete frame ({len(data)}/{total}), saving anyway")

    # save: 4-byte W, 4-byte H, then raw RGB bytes
    with open(out_path, "wb") as f:
        f.write(struct.pack(">II", w, h))
        f.write(data)
    print(f"[capture] saved to {out_path}")
    return out_path


# ---------------------------------------------------------------------------
# Load raw frame
# ---------------------------------------------------------------------------

def load_raw_frame(path):
    with open(path, "rb") as f:
        w, h = struct.unpack(">II", f.read(8))
        data = f.read(w * h * 3)
    arr = np.frombuffer(data, dtype=np.uint8).reshape((h, w, 3)).copy()
    print(f"[load] {w}x{h} raw frame, R mean={arr[:,:,0].mean():.1f} "
          f"G mean={arr[:,:,1].mean():.1f} B mean={arr[:,:,2].mean():.1f}")
    return arr   # float32 H×W×3, values 0–255


# ---------------------------------------------------------------------------
# Apply WB and clamp
# ---------------------------------------------------------------------------

def apply_wb(raw, r_mul, g_mul, b_mul):
    out = raw.astype(np.float32).copy()
    out[:, :, 0] *= r_mul
    out[:, :, 1] *= g_mul
    out[:, :, 2] *= b_mul
    return np.clip(out, 0, 255).astype(np.uint8)


# ---------------------------------------------------------------------------
# Interactive tuner (Tkinter)
# ---------------------------------------------------------------------------

def run_tuner(raw, initial_r=1.8, initial_g=1.3, initial_b=1.1):
    import tkinter as tk
    from tkinter import ttk
    from PIL import Image, ImageTk

    H, W = raw.shape[:2]

    # Downscale for display if needed
    MAX_DIM = 800
    scale = min(MAX_DIM / W, MAX_DIM / H, 1.0)
    disp_w = int(W * scale)
    disp_h = int(H * scale)

    root = tk.Tk()
    root.title("jankencamera — WB tuner")
    root.configure(bg="#1a1a1a")

    # ---- image panel ----
    img_label = tk.Label(root, bg="#1a1a1a", bd=0)
    img_label.pack(pady=(12, 4))

    photo_ref = [None]  # keep reference alive

    def refresh(*_):
        r = r_var.get()
        g = g_var.get()
        b = b_var.get()
        wb = apply_wb(raw, r, g, b)
        img = Image.fromarray(wb, "RGB")
        if scale < 1.0:
            img = img.resize((disp_w, disp_h), Image.LANCZOS)
        tk_img = ImageTk.PhotoImage(img)
        photo_ref[0] = tk_img
        img_label.configure(image=tk_img)
        code_var.set(f"#define WB_R  {r:.2f}f\n#define WB_G  {g:.2f}f\n#define WB_B  {b:.2f}f")

    # ---- sliders ----
    ctrl = tk.Frame(root, bg="#1a1a1a")
    ctrl.pack(fill=tk.X, padx=20, pady=4)

    style = ttk.Style()
    style.theme_use("clam")
    style.configure("WB.Horizontal.TScale", background="#1a1a1a",
                    troughcolor="#333", sliderlength=18)

    FONT = ("Menlo", 11)
    LABEL_W = 6

    def make_slider(parent, label, color_hex, initial, row):
        var = tk.DoubleVar(value=initial)
        tk.Label(parent, text=label, fg=color_hex, bg="#1a1a1a",
                 font=FONT, width=LABEL_W, anchor="w").grid(
                 row=row, column=0, sticky="w", padx=(0, 6))
        s = ttk.Scale(parent, from_=0.5, to=4.0, orient="horizontal",
                      variable=var, style="WB.Horizontal.TScale",
                      command=refresh, length=340)
        s.grid(row=row, column=1, sticky="ew", padx=4)
        val_lbl = tk.Label(parent, textvariable=var, bg="#1a1a1a",
                           fg="#ccc", font=FONT, width=5)
        val_lbl.grid(row=row, column=2, padx=(4, 0))
        # round display
        def fmt(*_): val_lbl.configure(text=f"{var.get():.2f}")
        var.trace_add("write", fmt)
        return var

    r_var = make_slider(ctrl, "WB_R", "#ff6b6b", initial_r, 0)
    g_var = make_slider(ctrl, "WB_G", "#69db7c", initial_g, 1)
    b_var = make_slider(ctrl, "WB_B", "#74c0fc", initial_b, 2)
    ctrl.columnconfigure(1, weight=1)

    # ---- code output box ----
    code_var = tk.StringVar()
    code_frame = tk.Frame(root, bg="#111", bd=1, relief="flat")
    code_frame.pack(fill=tk.X, padx=20, pady=(6, 2))
    tk.Label(code_frame, text="  Copy into main.c →", bg="#111",
             fg="#888", font=("Menlo", 10), anchor="w").pack(
             fill=tk.X, padx=6, pady=(4, 0))
    code_lbl = tk.Label(code_frame, textvariable=code_var, bg="#111",
                        fg="#e8e8e8", font=("Menlo", 12), justify="left",
                        anchor="w")
    code_lbl.pack(fill=tk.X, padx=12, pady=(2, 8))

    # ---- buttons ----
    btn_row = tk.Frame(root, bg="#1a1a1a")
    btn_row.pack(pady=(4, 12))

    def copy_code():
        root.clipboard_clear()
        root.clipboard_append(code_var.get())
        copy_btn.configure(text="✓ copied!")
        root.after(1500, lambda: copy_btn.configure(text="Copy defines"))

    copy_btn = tk.Button(btn_row, text="Copy defines", command=copy_code,
                         bg="#2d2d2d", fg="#eee", font=FONT,
                         relief="flat", padx=12, pady=4, cursor="hand2")
    copy_btn.pack(side="left", padx=6)

    def save_preview():
        wb = apply_wb(raw, r_var.get(), g_var.get(), b_var.get())
        Image.fromarray(wb, "RGB").save("wb_preview.png")
        save_btn.configure(text="✓ saved wb_preview.png")
        root.after(2000, lambda: save_btn.configure(text="Save preview PNG"))

    save_btn = tk.Button(btn_row, text="Save preview PNG", command=save_preview,
                         bg="#2d2d2d", fg="#eee", font=FONT,
                         relief="flat", padx=12, pady=4, cursor="hand2")
    save_btn.pack(side="left", padx=6)

    # ---- channel histogram ----
    HIST_W, HIST_H = 256, 60
    hist_canvas = tk.Canvas(root, width=HIST_W * 2, height=HIST_H,
                            bg="#111", bd=0, highlightthickness=0)
    hist_canvas.pack(pady=(0, 12))

    def draw_histogram(*_):
        hist_canvas.delete("all")
        wb = apply_wb(raw, r_var.get(), g_var.get(), b_var.get())
        for ch, color in enumerate(["#ff6b6b", "#69db7c", "#74c0fc"]):
            counts, _ = np.histogram(wb[:, :, ch], bins=128, range=(0, 256))
            peak = counts.max() or 1
            for i, c in enumerate(counts):
                x = i * (HIST_W * 2 // 128)
                bar_h = int(c / peak * HIST_H)
                hist_canvas.create_rectangle(
                    x, HIST_H - bar_h, x + max(1, HIST_W * 2 // 128 - 1), HIST_H,
                    fill=color, outline="", stipple="gray50" if ch > 0 else "")

    r_var.trace_add("write", draw_histogram)
    g_var.trace_add("write", draw_histogram)
    b_var.trace_add("write", draw_histogram)

    # initial draw
    refresh()
    draw_histogram()

    root.mainloop()


# ---------------------------------------------------------------------------
# Sweep mode (no GUI needed, just saves a grid of images)
# ---------------------------------------------------------------------------

def run_sweep(raw, out_dir="wb_sweep"):
    from PIL import Image
    os.makedirs(out_dir, exist_ok=True)
    r_vals = [1.2, 1.5, 1.8, 2.2, 2.6]
    g_vals = [1.0, 1.3, 1.6]
    b_vals = [0.9, 1.1, 1.4]
    total = len(r_vals) * len(g_vals) * len(b_vals)
    n = 0
    for r in r_vals:
        for g in g_vals:
            for b in b_vals:
                wb = apply_wb(raw, r, g, b)
                name = f"r{r:.1f}_g{g:.1f}_b{b:.1f}.png"
                Image.fromarray(wb, "RGB").save(os.path.join(out_dir, name))
                n += 1
                print(f"\r[sweep] {n}/{total} {name}", end="", flush=True)
    print(f"\n[sweep] done — images in ./{out_dir}/")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    p = argparse.ArgumentParser(description="jankencamera WB tuner")
    sub = p.add_subparsers(dest="cmd")

    cap = sub.add_parser("capture", help="grab a raw frame over UART")
    cap.add_argument("--port", required=True, help="/dev/tty.usbserial-XXXX")
    cap.add_argument("--baud", type=int, default=115200)
    cap.add_argument("--out", default="frame.raw")
    cap.add_argument("--timeout", type=int, default=60)

    tune = sub.add_parser("tune", help="interactively tune WB on a saved frame")
    tune.add_argument("frame", help="path to .raw file from capture mode")
    tune.add_argument("--r", type=float, default=1.8)
    tune.add_argument("--g", type=float, default=1.3)
    tune.add_argument("--b", type=float, default=1.1)

    sweep = sub.add_parser("sweep", help="batch-export a grid of WB variants")
    sweep.add_argument("frame", help="path to .raw file from capture mode")
    sweep.add_argument("--out", default="wb_sweep")

    args = p.parse_args()

    if args.cmd == "capture":
        capture_raw_frame(args.port, args.baud, args.out, args.timeout)

    elif args.cmd == "tune":
        raw = load_raw_frame(args.frame)
        run_tuner(raw, args.r, args.g, args.b)

    elif args.cmd == "sweep":
        raw = load_raw_frame(args.frame)
        run_sweep(raw, args.out)

    else:
        p.print_help()
        print("""
QUICK START
-----------
1. Add to main.c (see comment in script):
     uart_send_raw_frame(rgb_buf, cfg.width, cfg.height);   // before apply_white_balance

2. Load binary onto SD card, boot Pi.

3. Capture one frame:
     python3 wb_tuner.py capture --port /dev/tty.usbserial-XXXX --out frame.raw

4. Tune interactively:
     python3 wb_tuner.py tune frame.raw

5. (Optional) batch sweep:
     python3 wb_tuner.py sweep frame.raw
""")