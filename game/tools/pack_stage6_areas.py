#!/usr/bin/env python3
"""pack_stage6_areas.py S6DIR [AREA...]: stage 6's seven areas (specs/stage6.md
2.1) as separate scenery sets data/stage6/aN (tools/stagepack.py), each in
its own native coordinates: native x = arcade x - XOFF, native y = arcade y
- YOFF (the area's block rows from world y 0), so the engine reads every
area as an ordinary stage (StageData stage6_data[area]).

Per area: the grid-5 rows and columns (tools/stagemap.py, cropped), the far
layer at half the camera from the far x / y the area starts at
(tools/farlayer.py FAR_X0 / FAR_YOFF), the camera of the research runs'
RAM dumps (S6DIR/ram*/: stage word 5, play mode, FF8374 = area; transition
frames filtered by the area's fixed camera y or x range), then
tools/stagepack.py, colpack.py and maptrim.py (metatiles, trimmed
collision)."""
import os, pickle, struct, subprocess, sys
from pathlib import Path

GAME = Path(__file__).resolve().parents[1]
# area: grid rows (first, count), block columns (first, end), XOFF, YOFF,
# palette dump, far TOP (rows above the camera-y 0 view), far width,
# camera filter (fixed y or x range).
AREAS = {
    0: dict(rows=(14, 2), cols=(0, 10), xoff=0, yoff=0, dump="vid/v001000.cps", top=0, width=1344, y=0),
    1: dict(rows=(11, 2), cols=(0, 10), xoff=0, yoff=768, dump="vid/v009800.cps", top=0, width=1384, y=768),
    2: dict(rows=(8, 2), cols=(0, 10), xoff=0, yoff=1536, dump="vidD2/v003000.cps", top=0, width=1344, y=1536),
    3: dict(rows=(5, 2), cols=(0, 10), xoff=0, yoff=2304, dump="vidD3b/v005000.cps", top=0, width=1384, y=2304),
    4: dict(rows=(2, 2), cols=(0, 2), xoff=0, yoff=3072, dump="vidD4b/v002004.cps", top=0, width=360, y=3072),
    5: dict(rows=(7, 9), cols=(12, 16), xoff=3072, yoff=0, dump="vidD6/v005000.cps", top=1040, width=664, x=(3072, 4096)),
    6: dict(rows=(2, 2), cols=(4, 6), xoff=1024, yoff=3072, dump="vidD7/v003450.cps", top=0, width=440, y=3072),
}


def run(cmd, env=None, log=None):
    e = dict(os.environ); e.update(env or {})
    r = subprocess.run(cmd, cwd=GAME, env=e, capture_output=True, text=True)
    if log: Path(log).write_text(r.stdout + r.stderr)
    if r.returncode: raise SystemExit(f"{cmd}: {r.stderr[-800:]}")
    return r.stdout


def area(s6, n, cams):
    a = AREAS[n]
    out = GAME / f"data/stage6/a{n}"; out.mkdir(parents=True, exist_ok=True)
    tmp = Path(s6) / "native" / f"a{n}"; tmp.mkdir(parents=True, exist_ok=True)
    dump = str(Path(s6) / a["dump"])
    r0, nr = a["rows"]; c0, c1 = a["cols"]
    # The rows, all 16 block columns, then the area's columns.
    run(["python3", "tools/stagemap.py", "5", str(r0), str(nr), str(tmp / "full")])
    cells = (tmp / "full.cells").read_bytes()
    rows = nr * 16
    w = len(cells) // 4 // rows                   # cells a row (stagemap: up to the last used block)
    crop = b"".join(cells[(r * w + c0 * 16) * 4:(r * w + min(c1 * 16, w)) * 4] for r in range(rows))
    (tmp / "area.cells").write_bytes(crop)
    # The far layer.
    run(["python3", "tools/farlayer.py", dump, str(tmp / "far.ppm"), "5"],
        env={"FAR_WIDTH": str(a["width"]), "FAR_TOP": str(a["top"]), "FAR_YOFF": str(a["yoff"] // 2), "FAR_X0": str(a["xoff"] // 2)})
    # The camera (native), "B tN xh xl yh yl" lines.
    lines, k = [], 0
    for d, f, x, y in cams[n]:
        if "y" in a and y != a["y"]: continue
        if "x" in a and not a["x"][0] <= x < a["x"][1]: continue
        x -= a["xoff"]; y -= a["yoff"]
        if x < 0 or y < 0: continue
        lines.append("B t%d %02x %02x %02x %02x 00 00" % (k, x >> 8, x & 255, y >> 8, y & 255)); k += 2
    (tmp / "cam.txt").write_text("\n".join(lines) + "\n")
    env = {"WORLD_TOP": str(rows * 16 - 1), "FAR_SHIFT": "1", "FAR_TOP": str(a["top"]), "BD_TILES": os.environ.get("BD_TILES", "460"),
           "BD_FIT": "3"}
    if os.environ.get("FG_MERGE"): env["FG_MERGE"] = os.environ["FG_MERGE"]
    log = run(["python3", "-u", "tools/stagepack.py", str(tmp / "area.cells"), str(nr), dump, str(tmp / "cam.txt"), str(tmp / "far.ppm"), str(out)],
              env=env, log=str(tmp / "pack.log"))
    run(["python3", "tools/colpack.py", str(tmp / "area.cells"), str(nr), str(out)])
    run(["python3", "tools/maptrim.py", "--col", str(rows), str(out / "col.bin"), str(out / "col_trim.bin")])
    run(["python3", "tools/maptrim.py", "--meta", str(rows * 2), str(out / "map.bin"), str(out / "map_meta.bin")])
    print(f"area {n}: {len(lines)} camera samples; " + Path(tmp / "pack.log").read_text().strip().splitlines()[-1])


def main():
    s6 = sys.argv[1]
    cams = pickle.load(open(Path(s6) / "native" / "cams.pkl", "rb"))
    which = [int(x) for x in sys.argv[2:]] or list(AREAS)
    for n in which: area(s6, n, cams)


if __name__ == "__main__":
    main()
