#!/usr/bin/env python3
"""stage.py N OUTDIR [VAR=val ...]: repack stage N's scenery from its
decode.cache (dump, rows) into OUTDIR with the stage's recorded settings plus
overrides; the far picture via tools/farlayer.py from the cached dump; a
camera sweep synthesised from the cached rows (weights only)."""
import os, pickle, shutil, subprocess, sys
from pathlib import Path

G = Path("/Users/rester159/genesis ports/_capcom/willow/game")
N = int(sys.argv[1]); out = Path(sys.argv[2]); over = dict(a.split("=", 1) for a in sys.argv[3:])
STAGES = {
    2: dict(rows=2, far=dict(grid=1, FAR_WIDTH="4096", FAR_TOP="136"),
            env=dict(SPLIT_LINES="1", FAR_TOP="136", BD_TILES="339", POOL_SLOTS="687", FAR_WRAP="1")),
    3: dict(rows=2, far=dict(grid=2, FAR_WIDTH="3264", FAR_TOP="144", FAR_YOFF="128"),
            env=dict(POOL_SLOTS="687", FAR_TOP="144", BD_FIT="6", FG_MERGE="320", BD_TILES="339",
                     CYCLE=str(G / "data/stage3/cycle.json"))),
    4: dict(rows=32, far=dict(grid=3, FAR_WIDTH="256", FAR_TOP="670"),
            env=dict(WORLD_TOP="8191", FAR_SHIFT="3", FAR_WRAP="1", FAR_TOP="670")),
    5: dict(rows=3, far=dict(grid=4, FAR_WIDTH="3232", FAR_TOP="216"),
            env=dict(WORLD_TOP="767", BD_TILES="520", BD_FIT="3")),
}
st = STAGES[N]
out.mkdir(parents=True, exist_ok=True)
cache = G / f"data/stage{N}/decode.cache"
if not (out / "decode.cache").exists():
    shutil.copy(cache, out / "decode.cache")
k, _ = pickle.loads(cache.read_bytes())
(out / "dump.cps").write_bytes(k[1])
vis = k[2]
far = out / "far.ppm"
if not far.exists():
    e = dict(os.environ); e.update({a: b for a, b in st["far"].items() if a != "grid"})
    subprocess.run(["python3", str(G / "tools/farlayer.py"), str(out / "dump.cps"), str(far), str(st["far"]["grid"])], env=e, check=True)
env = dict(st["env"]); env.update(over)
WT = int(env.get("WORLD_TOP", "511"))
# Camera sweep: every 4th column, the top of the column's seen rows.
lines = []
for c in range(0, len(vis), 4):
    if not vis[c]:
        continue
    r0, r1 = vis[c]
    x0 = max(32, c * 8 - 160)
    for y0 in range(r0 * 8 + 48, max(r0 * 8 + 49, r1 * 8 - 48 - 224 + 1), 64):
        cx, cy = x0 - 32, WT - 239 - y0
        if WT == 511 and cy > 0x100:
            continue
        cy &= 0xFFFF
        lines.append("B %d %02x %02x %02x %02x" % (len(lines), cx >> 8 & 255, cx & 255, cy >> 8 & 255, cy & 255))
(out / "cam.txt").write_text("\n".join(lines) + "\n")
e = dict(os.environ); e.update(env); e["VIS_FROM_CACHE"] = "1"
r = subprocess.run(["python3", "-u", str(G / "tools/stagepack.py"), str(G / f"data/stage{N}_fg.cells"), str(st["rows"]),
                    str(out / "dump.cps"), str(out / "cam.txt"), str(far), str(out)], env=e, capture_output=True, text=True, cwd=out)
(out / "log.txt").write_text(r.stdout + r.stderr)
print((r.stdout + r.stderr).strip().split("\n")[-1])
for f in ("tiles.bin", "map.bin", "pal.bin", "bd_tiles.bin", "bd_map.bin", "loads.bin", "layout.h"):
    a, b = out / f, G / f"data/stage{N}/{f}"
    same = a.exists() and b.exists() and a.read_bytes() == b.read_bytes()
    print(f"  {f}: {'same' if same else 'differs'} ({a.stat().st_size if a.exists() else 0} vs {b.stat().st_size})")
