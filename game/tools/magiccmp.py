#!/usr/bin/env python3
"""magiccmp.py ARCADE.mag NATIVE: compare the charge state and the shots
frame by frame (frame k of the native run against arcade frame k-1, as
tracecmp --shift=1).
ARCADE: lines from the reference (tests/traces/NAME.mag): "frame cnt N spk
N cd N pend N meter HHHH act N rel HH | slot:aAkKvVsSxXyY ..." (class-8
slots: kind 1 = normal shot).
NATIVE: "frame MAGIC:<hex Magic struct> SHOTS:<hex 4 Shot structs>".
--tolerant: leave out shots past the screen's right edge (x > camera x + 352,
camera from NAME.ref; the arcade tests the bounds every 4th update on a
phase set by its frame counter and pool slot) and count a 1 px difference
in x as equal (the arcade's spawn keeps the pool slot's old sub-pixel
fraction). Both are accepted approximations (M1_NOTES.md)."""
import sys

def arcade(path):
    out = {}
    for l in open(path):
        head, _, tail = l.partition("|")
        p = head.split(); f = int(p[0]); d = dict(zip(p[1::2], p[2::2]))
        shots = []
        for s in tail.split():
            q = s.split(":")[1]
            k = int(q[q.index("k") + 1:q.index("v")]); v = int(q[q.index("v") + 1:q.index("s")])
            x = int(q[q.index("x") + 1:q.index("y")]); y = int(q[q.index("y") + 1:])
            if k == 1: shots.append((v, x, y))
        out[f] = (int(d["cnt"]), int(d["meter"], 16), int(d["act"]), int(d["rel"], 16), tuple(sorted(shots)))
    return out

def native(path):
    out = {}
    for l in open(path):
        f, m, s = l.split()[:3]
        m = bytes.fromhex(m.split(":")[1]); s = bytes.fromhex(s.split(":")[1])
        shots = []
        for i in range(4):
            r = s[i * 24:(i + 1) * 24]
            if int.from_bytes(r[10:12], "big"):
                shots.append((int.from_bytes(r[20:22], "big"), int.from_bytes(r[0:4], "big", signed=True) >> 8,
                              int.from_bytes(r[4:8], "big", signed=True) >> 8))
        out[int(f) - 1] = (m[5], int.from_bytes(m[0:2], "big"), m[2], m[3], tuple(sorted(shots)))
    return out

a, n = arcade(sys.argv[1]), native(sys.argv[2])
if "--tolerant" in sys.argv:
    cam = {}
    for l in open(sys.argv[1][:-4] + ".ref"):
        f, _, c = l.split()
        cam[int(f)] = int(c.split(":")[1][0:4], 16)
    def trim(f, v):
        return v[:4] + (tuple(t for t in v[4] if t[1] <= cam.get(f, 0) + 352),)
    a = {f: trim(f, v) for f, v in a.items()}
    n = {f: trim(f, v) for f, v in n.items()}
    for f in n:
        if f in a and len(a[f][4]) == len(n[f][4]) and all(
                p[0] == q[0] and p[2] == q[2] and abs(p[1] - q[1]) <= 1 for p, q in zip(a[f][4], n[f][4])):
            n[f] = n[f][:4] + (a[f][4],)
frames = sorted(set(a) & set(n))
names = ("hold", "meter", "active", "release", "shots")
bad = {k: [] for k in names}
for f in frames:
    for k, va, vn in zip(names, a[f], n[f]):
        if va != vn: bad[k].append((f, va, vn))
print(f"{len(frames)} frames compared")
for k in names:
    b = bad[k]
    print(f"{k:8s} {len(frames) - len(b):4d} match, {len(b):3d} differ" + (f"; first at frame {b[0][0]}: arcade {b[0][1]} native {b[0][2]}" if b else ""))
if "--all" in sys.argv:
    for f in frames:
        if a[f] != n[f]: print(f, a[f], n[f])
