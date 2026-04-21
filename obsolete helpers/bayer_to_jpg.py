import sys
import math
import numpy as np
from PIL import Image

INPUT = "imgs.txt"

# try common aspect ratios in order of preference
ASPECT_RATIOS = [(4, 3), (16, 9), (3, 2), (1, 1)]

def infer_dims(n):
    """Given n total pixel values, find (H, W) matching a common aspect ratio."""
    for aw, ah in ASPECT_RATIOS:
        h = math.sqrt(n * ah / aw)
        w = n / h
        if abs(h - round(h)) < 1e-6 and abs(w - round(w)) < 1e-6:
            return int(round(h)), int(round(w))
    # fallback: find the most square-ish factor pair
    best = None
    for h in range(1, int(math.sqrt(n)) + 1):
        if n % h == 0:
            w = n // h
            score = abs(math.log(w / h))  # 0 = square, lower = better
            if best is None or score < best[0]:
                best = (score, h, w)
    return best[1], best[2]

fname = sys.argv[1] if len(sys.argv) > 1 else INPUT

with open(fname) as f:
    lines = f.readlines()

# collect candidate lines: long lines of all integers
candidates = []
for line in lines:
    parts = line.strip().split()
    if parts and all(p.isdigit() for p in parts[:16]):
        candidates.append(parts)

# group lines that all have the same length (most common = image data)
if not candidates:
    print(f"no numeric data found in {fname}")
    sys.exit(1)

from collections import Counter
length_counts = Counter(len(p) for p in candidates)
img_len, _ = length_counts.most_common(1)[0]
img_lines = [p for p in candidates if len(p) == img_len]

H, W = infer_dims(img_len)
print(f"detected {len(img_lines)} image(s), {img_len} values each -> {W}x{H}")

for idx, parts in enumerate(img_lines):
    bayer = np.array(parts, dtype=np.uint16).reshape(H, W)
    gray = (bayer >> 2).astype(np.uint8)  # 10-bit -> 8-bit
    out = f"image_{idx}.jpg"
    Image.fromarray(gray, 'L').save(out)
    print(f"saved {out}")
