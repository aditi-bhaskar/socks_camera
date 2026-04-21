#!/usr/bin/env python3
# qoi-to-jpg.py — convert the QOI images the Aditi Pi saves (IMGnnnnn.QOI) to
# JPGs. QOI is lossless and self-describing (it carries its own width/height),
# so unlike the .565 files there's nothing to configure.
#
# Pure-Python decoder (no `pip install qoi` needed). Spec: https://qoiformat.org
#
# usage:
#   python3 qoi-to-jpg.py        # converts every .QOI sitting next to this script
#
# Each IMGnnnnn.QOI becomes IMGnnnnn.jpg right beside it in this directory.
#
# deps: numpy, pillow (the laptop_code venv works)

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


def convert_file(path):
    with open(path, "rb") as f:
        data = f.read()
    rgb = decode_qoi(data)
    out_path = os.path.splitext(path)[0] + ".jpg"
    Image.fromarray(rgb).save(out_path, "JPEG", quality=95)
    print(f"  {os.path.basename(path)} -> {os.path.basename(out_path)} "
          f"({rgb.shape[1]}x{rgb.shape[0]})")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    files = sorted(
        os.path.join(here, name)
        for name in os.listdir(here)
        if name.lower().endswith(".qoi")
    )
    if not files:
        print(f"no .QOI files found in {here}", file=sys.stderr)
        sys.exit(1)

    print(f"converting {len(files)} file(s) in {here} ...")
    ok = 0
    for path in files:
        try:
            convert_file(path)
            ok += 1
        except Exception as e:
            print(f"  SKIP {os.path.basename(path)}: {e}", file=sys.stderr)
    print(f"done: {ok}/{len(files)} converted")


if __name__ == "__main__":
    main()
