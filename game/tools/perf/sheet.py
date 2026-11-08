#!/usr/bin/env python3
"""sheet.py OUT.png COLS ppm...: tile Genesis frame dumps at half size."""
import subprocess, sys, os
out, cols, fs = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = []
for f in fs:
    d = open(f, "rb").read(); parts = d.split(b"\n", 3); w, h = map(int, parts[1].split()); ims.append((w, h, parts[3]))
w, h = ims[0][0], ims[0][1]; W = w // 2 * cols; H = h // 2 * ((len(ims) + cols - 1) // cols)
o = bytearray(W * H * 3)
for k, (ww, hh, px) in enumerate(ims):
    ox = (k % cols) * (w // 2); oy = (k // cols) * (h // 2)
    for y in range(h // 2):
        for x in range(w // 2):
            i = ((y * 2) * w + x * 2) * 3; j = ((oy + y) * W + ox + x) * 3; o[j:j + 3] = px[i:i + 3]
open(out + ".ppm", "wb").write(b"P6\n%d %d\n255\n" % (W, H) + o)
subprocess.run(["sips", "-s", "format", "png", out + ".ppm", "--out", out], check=True, capture_output=True); os.remove(out + ".ppm")
