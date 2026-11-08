#!/usr/bin/env python3
"""maptrim.py [--col ROWS | --meta ROWS] IN.bin OUT.bin: a stage's column-major grid with each
column trimmed to its non-empty rows (src/stage.c, src/actor.c read through it).

Default: the plane-A map (tools/stagepack.py: u16 words, 64 rows a column, or
ROWS=n words a column with --rows n). --col ROWS: the collision grid
(tools/colpack.py: u8 ids, ROWS a column).
OUT: per column {u32 offset (elements into the data), u16 first row, u16 rows},
then the data (u16 words / u8 ids, big-endian).
--meta ROWS: the plane-A map as 16x16 metatiles (the stages' map_meta.bin,
what src/stage.c reads; see below)."""
import struct, sys
from pathlib import Path

args = sys.argv[1:]
if args[0] == "--meta":
    # The plane-A map as 16x16 metatiles: a u32 word offset of the dictionary,
    # then per 16-px column {u32 offset (indices into the data), u16 first
    # 16-px row, u16 rows}, the u16 metatile indices, the dictionary (4
    # words a metatile: top-left, top-right, bottom-left, bottom-right; 0 =
    # empty). src/stage.c map_word() reads it.
    rows = int(args[1]); args = args[2:]
    d = Path(args[0]).read_bytes()
    n = len(d) // (rows * 2)
    blocks = {(0, 0, 0, 0): 0}
    index, data = [], []
    for cc in range(n // 2):
        a = struct.unpack_from(">%dH" % rows, d, 2 * cc * rows * 2)
        b = struct.unpack_from(">%dH" % rows, d, (2 * cc + 1) * rows * 2)
        ids = []
        for rr in range(rows // 2):
            k = (a[2 * rr], b[2 * rr], a[2 * rr + 1], b[2 * rr + 1])
            if k not in blocks: blocks[k] = len(blocks)
            ids.append(blocks[k])
        nz = [i for i, v in enumerate(ids) if v]
        first, cnt = (nz[0], nz[-1] - nz[0] + 1) if nz else (0, 0)
        index.append((len(data), first, cnt))
        data += ids[first:first + cnt]
    head = 2 + len(index) * 4
    dict_at = head + len(data)
    out = struct.pack(">I", dict_at) + b"".join(struct.pack(">IHH", *e) for e in index) + struct.pack(">%dH" % len(data), *data)
    for k in sorted(blocks, key=blocks.get): out += struct.pack(">4H", *k)
    Path(args[1]).write_bytes(out)
    print(f"{args[0]}: {len(d)} -> {len(out)} bytes ({len(blocks)} metatiles)")
    sys.exit()
col = args[0] == "--col"
rows = 64
if col or args[0] == "--rows":
    rows = int(args[1]); args = args[2:]
d = Path(args[0]).read_bytes()
size = 1 if col else 2
n = len(d) // (rows * size)
index, data = [], []
for c in range(n):
    if col: w = d[c * rows:(c + 1) * rows]
    else: w = struct.unpack_from(">%dH" % rows, d, c * rows * 2)
    nz = [i for i, v in enumerate(w) if v]
    first, cnt = (nz[0], nz[-1] - nz[0] + 1) if nz else (0, 0)
    index.append((len(data), first, cnt))
    data += list(w[first:first + cnt])
out = b"".join(struct.pack(">IHH", *e) for e in index)
out += bytes(data) if col else struct.pack(">%dH" % len(data), *data)
if len(out) & 1: out += b"\0"
Path(args[1]).write_bytes(out)
print(f"{args[0]}: {len(d)} -> {len(out)} bytes ({n} columns)")
