#!/usr/bin/env python3
"""streamcheck.py STAGEDIR FRAME.ppm...: compare frames from the CAMTEST
build with the offline picture (stagepack preview over the backdrop
preview) at the best-matching view position near the logged camera path.
A correct stream matches every pixel between the HUD rows (colours within
the DAC rounding) except the test build's Willow sprite, the lives sprites
and, while a palette cycle runs, its cycling pixels (the preview shows its
first state)."""
import struct, sys
from pathlib import Path

def ppm(p):
    d = Path(p).read_bytes().split(b"\n", 3)
    w, h = map(int, d[1].split())
    return w, h, d[3]

sd = Path(sys.argv[1])
MW, MH, M = ppm(sd / "preview.ppm")
_, _, BD = ppm(sd / "bd_preview.ppm")
cam = Path(sd / "camtrace.bin").read_bytes()
path = [struct.unpack_from(">hh", cam, 4 * i) for i in range(len(cam) // 4)]
# Compare Genesis levels: the preview writes level * 36, the emulator's DAC
# level * 34 (7 -> 238).
close = lambda a, b: all(x // 36 == (y + 17) // 34 for x, y in zip(a, b))
HUD_TOP, HUD_BOTTOM = 16, 208       # rows the HUD covers (top text, bottom band)
# Palette cycle colours (stagepack cycle.bin): a pixel showing one of them on
# both sides is taken as a cycling pixel in another state.
cyc = set()
if (sd / "cycle.bin").exists():
    cb = (sd / "cycle.bin").read_bytes()
    for i in range(0, len(cb), 2):
        w = int.from_bytes(cb[i:i + 2], "big")
        cyc.add(((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7))
lv_pre = lambda a: tuple(x // 36 for x in a)
lv_emu = lambda a: tuple((x + 17) // 34 for x in a)
_close = close
close = lambda a, b: _close(a, b) or (lv_pre(a) in cyc and lv_emu(b) in cyc)
for f in sys.argv[2:]:
    w, h, F = ppm(f)
    n = int(Path(f).stem[1:])
    best = None
    for k in range(max(0, n - 36), min(len(path), n - 22)):   # init delay: about 29 frames
        vx, vy = path[k]
        for dx in range(-16, 17):
            x0 = vx + dx
            if x0 < 0 or x0 + w > MW:
                continue
            bad = 0
            for y in range(8, h, 7):
                for x in range(0, w, 7):
                    i = ((vy + y) * MW + x0 + x) * 3
                    e = M[i:i + 3]
                    if e == b"\0\0\0":
                        e = BD[(y * w + x) * 3:][:3]
                    if not close(e, F[(y * w + x) * 3:][:3]):
                        bad += 1
            if best is None or bad < best[0]:
                best = (bad, k, x0, vy)
            if bad == 0:
                break
        if best and best[0] == 0:
            break
    bad, k, x0, vy = best
    # full check at the best position
    bad = sum(1 for y in range(HUD_TOP, HUD_BOTTOM) for x in range(w)
              if not close(M[((vy + y) * MW + x0 + x) * 3:][:3] if M[((vy + y) * MW + x0 + x) * 3:][:3] != b"\0\0\0"
                           else BD[(y * w + x) * 3:][:3], F[(y * w + x) * 3:][:3]))
    print(f"{Path(f).name}: view ({x0}, {vy}) path frame {k}: {bad} of {w * (HUD_BOTTOM - HUD_TOP)} pixels "
          f"between the HUD rows differ")
