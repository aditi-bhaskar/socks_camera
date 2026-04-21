#!/usr/bin/env python3
"""
pixel_editor.py — draw a sprite on a grid, preview it rotated, copy C code.

Usage:
    python3 pixel_editor.py [--w 12] [--h 16]

Controls:
    Left-click / drag   toggle pixels on
    Right-click / drag  erase pixels
    R / L buttons       rotate preview ±15 degrees
    Clear               wipe the grid
    Copy C code         copies a C uint8_t bitmap[][] to clipboard
"""

import argparse
import math
import tkinter as tk

CELL = 24        # pixels per grid cell in the editor
PREVIEW_SCALE = 4  # scale factor for the preview panel

def make_app(W, H):
    grid = [[0]*W for _ in range(H)]
    angle_deg = [15]   # mutable so inner functions can write it

    root = tk.Tk()
    root.title("pixel editor → C code")
    root.configure(bg="#1a1a1a")
    root.resizable(False, False)

    # ── editor canvas ──────────────────────────────────────────────────────
    canvas_w = W * CELL + 1
    canvas_h = H * CELL + 1
    canvas = tk.Canvas(root, width=canvas_w, height=canvas_h,
                       bg="#111", highlightthickness=0)
    canvas.grid(row=0, column=0, padx=(12,6), pady=12)

    def draw_grid():
        canvas.delete("all")
        for y in range(H):
            for x in range(W):
                x0, y0 = x*CELL, y*CELL
                fill = "#00d4ff" if grid[y][x] else "#1e1e1e"
                canvas.create_rectangle(x0, y0, x0+CELL, y0+CELL,
                                        fill=fill, outline="#333")

    def cell_at(ex, ey):
        x, y = ex // CELL, ey // CELL
        if 0 <= x < W and 0 <= y < H:
            return x, y
        return None, None

    painting = [None]   # True=paint, False=erase

    def on_press(e):
        x, y = cell_at(e.x, e.y)
        if x is None: return
        painting[0] = not grid[y][x]   # first click determines paint/erase
        grid[y][x] = 1 if painting[0] else 0
        draw_grid(); update_preview()

    def on_drag(e):
        if painting[0] is None: return
        x, y = cell_at(e.x, e.y)
        if x is None: return
        grid[y][x] = 1 if painting[0] else 0
        draw_grid(); update_preview()

    def on_release(e):
        painting[0] = None

    canvas.bind("<ButtonPress-1>",   on_press)
    canvas.bind("<B1-Motion>",       on_drag)
    canvas.bind("<ButtonRelease-1>", on_release)
    canvas.bind("<ButtonPress-3>",   lambda e: (on_press.__func__ if False else None) or
                                               _erase_press(e))
    canvas.bind("<B3-Motion>",       on_drag)

    def _erase_press(e):
        x, y = cell_at(e.x, e.y)
        if x is None: return
        painting[0] = False
        grid[y][x] = 0
        draw_grid(); update_preview()

    # ── preview canvas (shows rotated result) ──────────────────────────────
    OLED_W, OLED_H = 128, 64
    prev_scale = 3
    prev_canvas = tk.Canvas(root,
                            width=OLED_W*prev_scale, height=OLED_H*prev_scale,
                            bg="#000", highlightthickness=1,
                            highlightbackground="#444")
    prev_canvas.grid(row=0, column=1, padx=(6,12), pady=12, sticky="n")

    tk.Label(root, text="OLED preview (128×64)", bg="#1a1a1a",
             fg="#888", font=("Menlo", 10)).grid(row=1, column=1)

    def update_preview():
        prev_canvas.delete("all")
        ang = math.radians(angle_deg[0])
        cos_a, sin_a = math.cos(ang), math.sin(ang)
        cx, cy = W/2, H/2

        def draw_sock_at(ox, oy):
            for y in range(H):
                for x in range(W):
                    if not grid[y][x]: continue
                    dx, dy = x - cx, y - cy
                    rx = int(round(dx*cos_a - dy*sin_a))
                    ry = int(round(dx*sin_a + dy*cos_a))
                    px = ox + int(cx) + rx
                    py = oy + int(cy) + ry
                    if 0 <= px < OLED_W and 0 <= py < OLED_H:
                        prev_canvas.create_rectangle(
                            px*prev_scale, py*prev_scale,
                            (px+1)*prev_scale, (py+1)*prev_scale,
                            fill="#00d4ff", outline="")

        draw_sock_at(1, 1)                    # top-left  +angle
        # mirror for top-right: negate angle
        ang2 = math.radians(-angle_deg[0])
        cos_b, sin_b = math.cos(ang2), math.sin(ang2)

        def draw_sock_mirrored(ox, oy):
            for y in range(H):
                for x in range(W):
                    if not grid[y][x]: continue
                    dx, dy = x - cx, y - cy
                    rx = int(round(dx*cos_b - dy*sin_b))
                    ry = int(round(dx*sin_b + dy*cos_b))
                    px = ox + int(cx) + rx
                    py = oy + int(cy) + ry
                    if 0 <= px < OLED_W and 0 <= py < OLED_H:
                        prev_canvas.create_rectangle(
                            px*prev_scale, py*prev_scale,
                            (px+1)*prev_scale, (py+1)*prev_scale,
                            fill="#00d4ff", outline="")

        draw_sock_mirrored(OLED_W - W - 1, OLED_H - H - 1)  # bottom-right -angle

        # Render text via PIL at size 8 (close to the 6px OLED font) and
        # blit each lit pixel onto the preview as a small rectangle.
        try:
            from PIL import Image, ImageDraw, ImageFont
            img = Image.new("1", (OLED_W, OLED_H), 0)
            draw_img = ImageDraw.Draw(img)
            font = ImageFont.load_default()
            l1, l2 = "aditi and christy", "capture the world"
            # PIL default font is ~6px wide per char
            w1 = len(l1) * 6; w2 = len(l2) * 6
            draw_img.text(((OLED_W - w1)//2, 22), l1, fill=1, font=font)
            draw_img.text(((OLED_W - w2)//2, 32), l2, fill=1, font=font)
            for py in range(OLED_H):
                for px in range(OLED_W):
                    if img.getpixel((px, py)):
                        prev_canvas.create_rectangle(
                            px*prev_scale, py*prev_scale,
                            (px+1)*prev_scale, (py+1)*prev_scale,
                            fill="#444", outline="")
        except Exception:
            prev_canvas.create_text(
                OLED_W*prev_scale//2, OLED_H*prev_scale//2,
                text="aditi and christy\ncapture the world",
                fill="#444", font=("Courier", 8), justify="center")

    # ── controls ───────────────────────────────────────────────────────────
    ctrl = tk.Frame(root, bg="#1a1a1a")
    ctrl.grid(row=2, column=0, columnspan=2, pady=(0,8), padx=12, sticky="ew")

    FONT = ("Menlo", 11)
    BTN = dict(bg="#2d2d2d", fg="#eee", font=FONT,
               relief="flat", padx=10, pady=4, cursor="hand2")

    def clear_grid():
        for y in range(H):
            for x in range(W):
                grid[y][x] = 0
        draw_grid(); update_preview()

    def change_angle(delta):
        angle_deg[0] = max(-45, min(45, angle_deg[0] + delta))
        angle_lbl.configure(text=f"angle: {angle_deg[0]:+d}°")
        update_preview()

    tk.Button(ctrl, text="◀ angle",  command=lambda: change_angle(-5), **BTN).pack(side="left", padx=4)
    tk.Button(ctrl, text="angle ▶",  command=lambda: change_angle(+5), **BTN).pack(side="left", padx=4)
    angle_lbl = tk.Label(ctrl, text=f"angle: {angle_deg[0]:+d}°",
                         bg="#1a1a1a", fg="#aaa", font=FONT)
    angle_lbl.pack(side="left", padx=8)

    tk.Button(ctrl, text="Clear", command=clear_grid, **BTN).pack(side="left", padx=4)

    copy_btn = tk.Button(ctrl, text="Copy C code", **BTN)
    copy_btn.pack(side="right", padx=4)

    def copy_c():
        lines = [f"// {W}x{H} sprite bitmap",
                 f"#define SOCK_W {W}",
                 f"#define SOCK_H {H}",
                 f"static const uint8_t sock_bmp[SOCK_H][SOCK_W] = {{"]
        for y in range(H):
            row = "{" + ",".join(str(grid[y][x]) for x in range(W)) + "},"
            lines.append("    " + row)
        lines.append("};")
        code = "\n".join(lines)
        root.clipboard_clear()
        root.clipboard_append(code)
        copy_btn.configure(text="✓ copied!")
        root.after(1500, lambda: copy_btn.configure(text="Copy C code"))

    copy_btn.configure(command=copy_c)

    # ── size label ─────────────────────────────────────────────────────────
    tk.Label(ctrl, text=f"grid: {W}×{H}", bg="#1a1a1a",
             fg="#555", font=FONT).pack(side="right", padx=8)

    draw_grid()
    update_preview()
    root.mainloop()


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--w", type=int, default=12, help="grid width in pixels")
    p.add_argument("--h", type=int, default=16, help="grid height in pixels")
    args = p.parse_args()
    make_app(args.w, args.h)
