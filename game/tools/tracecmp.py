#!/usr/bin/env python3
"""tracecmp.py ARCADE NATIVE: compare Willow's movement frame by frame.
ARCADE: lines "frame 9846:<hex player record> 8408:<hex camera>" from the
B1 cartridge (the original program, frame-identical) under a script;
NATIVE: lines "frame 02d8:<hex Player struct>" from this game under the
same script (frame 0 = first scripted input in both).
B1 runs the arcade program slower than 60 updates/s, so some VBlanks show
no update (the whole record, including the pose tick, unchanged): those are
dropped and the arcade is compared by update count."""
import json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
nodes = [n["source"] for n in json.loads((ROOT / "build/native-world/player-hold-data-v1/manifest.json").read_text())["nodes"]]
pose_of = {a: i for i, a in enumerate(nodes)}

def arcade(path):
    out = {}
    for l in open(path):
        f, rec, cam = l.split()
        r = bytes.fromhex(rec.split(":")[1]); c = bytes.fromhex(cam.split(":")[1])
        x = int.from_bytes(r[6:8], "big"); y = int.from_bytes(r[10:12], "big")
        fac = r[0x0F]; pose = pose_of.get(int.from_bytes(r[0x14:0x18], "big"), -1)
        out[int(f)] = (x, y, fac, pose, int.from_bytes(c[0:2], "big"), int.from_bytes(c[4:6], "big"), rec + cam)
    if not DEDUPE:
        return {f: v[:6] for f, v in out.items()}
    upd, prev, k = {}, None, 0
    for f in sorted(out):
        if f > 0 and out[f][6] == prev:
            continue                                  # no arcade update this VBlank
        prev = out[f][6]
        upd[k if f > 0 else f] = out[f][:6]
        if f > 0: k += 1
        if f == 0: k = 1
    return upd

def native(path):
    out = {}
    for l in open(path):
        f, rec = l.split()[:2]
        r = bytes.fromhex(rec.split(":")[1])
        x = int.from_bytes(r[0:4], "big", signed=True) >> 8; y = int.from_bytes(r[4:8], "big", signed=True) >> 8
        fac = int.from_bytes(r[22:24], "big"); pose = int.from_bytes(r[24:26], "big")
        camx = int.from_bytes(r[32:34], "big", signed=True)
        camy = int.from_bytes(r[34:36], "big", signed=True)
        out[int(f)] = (x, y, fac, pose, camx, camy)
    return out

args = [x for x in sys.argv[1:] if not x.startswith("--")]
DEDUPE = "--b1" in sys.argv                      # B1 trace: drop VBlanks without an update
SHIFT = next((int(x[8:]) for x in sys.argv if x.startswith("--shift=")), 0)
a, n = arcade(args[0]), native(args[1])
n = {f - SHIFT: v for f, v in n.items()}
frames = sorted(set(a) & set(n))
names = ("x", "y", "facing", "pose", "camera x", "camera y")
bad = {k: [] for k in names}
for f in frames:
    for k, va, vn in zip(names, a[f], n[f]):
        if va != vn:
            bad[k].append((f, va, vn))
print(f"{len(frames)} frames compared")
for k in names:
    b = bad[k]
    print(f"{k:9s} {len(frames) - len(b):4d} match, {len(b):3d} differ" + (f"; first at frame {b[0][0]}: arcade {b[0][1]} native {b[0][2]}" if b else ""))
if "--all" in sys.argv:
    for f in frames:
        if any(x != y for x, y in zip(a[f], n[f])):
            print(f, a[f], n[f])
