#!/usr/bin/env python3
"""area6.py N OUTDIR [VAR=val ...]: repack stage 6 area N's scenery from its
decode.cache (dump, rows) as tools/pack_stage6_areas.py does, into OUTDIR
(camera sweep synthesised from the cached rows: weights only)."""
import os, pickle, shutil, struct, subprocess, sys
from pathlib import Path

G = Path("/Users/rester159/genesis ports/_capcom/willow/game")
sys.path.insert(0, str(G / "tools"))
from pack_stage6_areas import AREAS  # noqa: E402

N = int(sys.argv[1]); out = Path(sys.argv[2]); over = dict(a.split("=", 1) for a in sys.argv[3:])
a = AREAS[N]
out.mkdir(parents=True, exist_ok=True)
src = G / f"data/stage6/a{N}"
if not (out / "decode.cache").exists():
    shutil.copy(src / "decode.cache", out / "decode.cache")
k, _ = pickle.loads((src / "decode.cache").read_bytes())
(out / "dump.cps").write_bytes(k[1])
r0, nr = a["rows"]; c0, c1 = a["cols"]
if not (out / "area.cells").exists():
    subprocess.run(["python3", "tools/stagemap.py", "5", str(r0), str(nr), str(out / "full")], cwd=G, check=True, capture_output=True)
    cells = (out / "full.cells").read_bytes()
    rows = nr * 16
    w = len(cells) // 4 // rows
    crop = b"".join(cells[(r * w + c0 * 16) * 4:(r * w + min(c1 * 16, w)) * 4] for r in range(rows))
    (out / "area.cells").write_bytes(crop)
assert (out / "area.cells").read_bytes() == k[0], "area cells differ from the cache"
if not (out / "far.ppm").exists():
    e = dict(os.environ, FAR_WIDTH=str(a["width"]), FAR_TOP=str(a["top"]), FAR_YOFF=str(a["yoff"] // 2), FAR_X0=str(a["xoff"] // 2))
    subprocess.run(["python3", "tools/farlayer.py", str(out / "dump.cps"), str(out / "far.ppm"), "5"], cwd=G, env=e, check=True)
rows = nr * 16
WT = rows * 16 - 1
vis = k[2]
lines = []
for c in range(0, len(vis), 4):
    if not vis[c]:
        continue
    r0_, r1_ = vis[c]
    x0 = max(32, c * 8 - 160)
    for y0 in range(r0_ * 8 + 48, max(r0_ * 8 + 49, r1_ * 8 - 48 - 224 + 1), 64):
        cx, cy = x0 - 32, WT - 239 - y0
        if WT == 511 and cy > 0x100:
            continue
        cy &= 0xFFFF
        lines.append("B t%d %02x %02x %02x %02x" % (len(lines), cx >> 8 & 255, cx & 255, cy >> 8 & 255, cy & 255))
(out / "cam.txt").write_text("\n".join(lines) + "\n")
env = {"WORLD_TOP": str(WT), "FAR_SHIFT": "1", "FAR_TOP": str(a["top"]), "BD_TILES": "460", "BD_FIT": "3", "VIS_FROM_CACHE": "1"}
w = struct.unpack(">32H", (src / "pal.bin").read_bytes())
def p(v): return "%d%d%d" % ((v >> 1) & 7, (v >> 5) & 7, (v >> 9) & 7)
env["PIN"] = "+".join(p(x) for x in w[1:16]) + "," + "+".join(p(x) for x in w[17:32])
env.update(over)
e = dict(os.environ); e.update(env)
r = subprocess.run(["python3", "-u", str(G / "tools/stagepack.py"), str(out / "area.cells"), str(nr), str(out / "dump.cps"),
                    str(out / "cam.txt"), str(out / "far.ppm"), str(out)], env=e, capture_output=True, text=True, cwd=out)
(out / "log.txt").write_text(r.stdout + r.stderr)
print(f"a{N}: " + (r.stdout + r.stderr).strip().split("\n")[-1])
for f in ("tiles.bin", "map.bin", "pal.bin", "bd_tiles.bin", "layout.h"):
    p, q = out / f, src / f
    print(f"  {f}: {'same' if p.exists() and p.read_bytes() == q.read_bytes() else 'differs'} ({p.stat().st_size if p.exists() else 0} vs {q.stat().st_size})")
