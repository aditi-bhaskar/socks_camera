#!/usr/bin/env python3
# qoi-to-png.py — convert the QOI images the Aditi Pi saves (IMGnnnnn.QOI) to
# PNGs. QOI is lossless and self-describing (it carries its own width/height),
# so unlike the .565 files there's nothing to configure.
#
# Pure-Python decoder (no `pip install qoi` needed). Spec: https://qoiformat.org
#
# usage:
#   python3 qoi-to-png.py /Volumes/ADITI_PI --out png      # whole card -> png/
#   python3 qoi-to-png.py IMG00007.QOI                     # one file
#
# NOTE: the Pi never cleanly unmounts the card, so if reading straight from
# /Volumes/... gives "Invalid argument", eject + reinsert the card first (or
# copy the files off), then run this.
#
# deps: numpy, pillow (the laptop_code venv works)

import argparse
import os
import sys

import numpy as np
from PIL import Image

QOI_OP_INDEX = 0x00
QOI_OP_DIFF  = 0x40
QOI_OP_LUMA  = 0x80
QOI_OP_RUN   = 0xC0
QOI_OP_RGB   = 0xFE
QOI_OP_RGBA  = 0xFF


def decode_qoi(data):
    if data[:4] != b"qoif":
        raise ValueError("not a QOI file (bad magic)")
    w = int.from_bytes(data[4:8], "big")
    h = int.from_bytes(data[8:12], "big")
    npx = w * h

    out = np.empty((npx, 3), dtype=np.uint8)
    index = [(0, 0, 0, 0)] * 64
    r, g, b, a = 0, 0, 0, 255
    p = 14
    run = 0

    for i in range(npx):
        if run > 0:
            run -= 1
        else:
            op = data[p]; p += 1
            if op == QOI_OP_RGB:
                r, g, b = data[p], data[p + 1], data[p + 2]; p += 3
            elif op == QOI_OP_RGBA:
                r, g, b, a = data[p], data[p + 1], data[p + 2], data[p + 3]; p += 4
            elif (op & 0xC0) == QOI_OP_INDEX:
                r, g, b, a = index[op & 0x3F]
            elif (op & 0xC0) == QOI_OP_DIFF:
                r = (r + ((op >> 4) & 3) - 2) & 0xFF
                g = (g + ((op >> 2) & 3) - 2) & 0xFF
                b = (b + (op & 3) - 2) & 0xFF
            elif (op & 0xC0) == QOI_OP_LUMA:
                op2 = data[p]; p += 1
                vg = (op & 0x3F) - 32
                r = (r + vg + ((op2 >> 4) & 0xF) - 8) & 0xFF
                g = (g + vg) & 0xFF
                b = (b + vg + (op2 & 0xF) - 8) & 0xFF
            elif (op & 0xC0) == QOI_OP_RUN:
                run = op & 0x3F            # this pixel + `run` more
        index[(r * 3 + g * 5 + b * 7 + a * 11) & 63] = (r, g, b, a)
        out[i] = (r, g, b)

    return out.reshape(h, w, 3)


def convert_file(path, out_dir):
    with open(path, "rb") as f:
        data = f.read()
    rgb = decode_qoi(data)
    base = os.path.splitext(os.path.basename(path))[0] + ".png"
    out_path = os.path.join(out_dir, base) if out_dir else \
        os.path.join(os.path.dirname(os.path.abspath(path)), base)
    Image.fromarray(rgb).save(out_path)
    print(f"  {path} -> {out_path} ({rgb.shape[1]}x{rgb.shape[0]})")


def gather_inputs(paths):
    files = []
    for p in paths:
        if os.path.isdir(p):
            for name in sorted(os.listdir(p)):
                if name.lower().endswith(".qoi"):
                    files.append(os.path.join(p, name))
        else:
            files.append(p)
    return files


def main():
    ap = argparse.ArgumentParser(description="Convert QOI (.QOI) captures to PNG.")
    ap.add_argument("paths", nargs="+", help=".QOI files and/or directories")
    ap.add_argument("--out", default=None, help="output directory (default: alongside each input)")
    args = ap.parse_args()

    if args.out:
        os.makedirs(args.out, exist_ok=True)

    files = gather_inputs(args.paths)
    if not files:
        print("no .QOI files found", file=sys.stderr)
        sys.exit(1)

    print(f"converting {len(files)} file(s)...")
    ok = 0
    for path in files:
        try:
            convert_file(path, args.out)
            ok += 1
        except Exception as e:
            print(f"  SKIP {path}: {e}", file=sys.stderr)
    print(f"done: {ok}/{len(files)} converted")


if __name__ == "__main__":
    main()
