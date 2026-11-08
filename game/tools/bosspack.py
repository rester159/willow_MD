#!/usr/bin/env python3
"""bosspack.py PALDUMP.cps STAGEDIR SPRPAL OUTDIR: the stage-1 boss body as
plane B pictures (specs/stage1_boss.md 3.3; sgdk-sprites-vram: a huge
actor as a plane body).

The arena's camera is fixed (x 3328, y 0) while the body is on screen, so
every body pose is composited offline into the cells it covers at its fixed
place (body origin 3600, 56): a cell's pixel is the body's pixel, else
transparent where the foreground (plane A) is opaque there, else the
backdrop picture's pixel. Cells with body pixels carry the priority bit
(over the foreground); the others keep their backdrop word. Each tile takes
the best of CRAM lines 1-3 (line 0 is Willow's).

Poses: intro 0x27222 (4 nodes), loop 0x27318 (8), death 0x27716 (4 +
empty). Writes (big-endian):
  body.bin   u16 region x0, y0, w, h (cells); u16 pose count; per pose u16
             tile count, u32 offset of its tiles, u32 offset of its map; maps:
             w*h u16 (0xFFFF: the backdrop word; else tile index | line << 13
             | priority); tiles 32 bytes each
  slots.bin  2 x BODY_MAX_TILES u16 VRAM tiles: the foreground pool slots
             free at the arena, the body's double buffer
  maps.bin   per buffer, per pose: the region's h x w plane words resolved
             (backdrop words and buffer slots filled in)
  preview.ppm  every pose as the Genesis will show it
"""
import struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

TABLE_SEL = 0x3B9B2
ORIGIN_SX, ORIGIN_SY = 3600 - (3328 + 32), (511 - 56) - (272 - 0)   # screen position of the body origin
CAM_COL, VIEW_TOP = (3328 + 32) // 8, 272                          # plane A view: map column, map y
WEIGHT = (2, 4, 3)


def chan(v):
    return min(7, (v + 16) >> 5)


def rgb3(w):
    bright = 0x0F + ((w >> 12) << 1)
    return tuple(chan(((w >> s) & 15) * 0x11 * bright // 0x2D) for s in (8, 4, 0))


def cram(w):
    return ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)


def dist(a, b):
    return sum(k * (x - y) ** 2 for k, x, y in zip(WEIGHT, a, b))


def main():
    dump_path, stage_dir, sprpal_path, outdir = sys.argv[1:5]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    sd = Path(stage_dir)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    u = lambda a: struct.unpack_from(">H", rom, a)[0]
    raw = Path(dump_path).read_bytes(); dump = raw[0x80:]
    pal_base = (int.from_bytes(raw[0x0A:0x0C], "big") << 8) - 0x900000
    pal = lambda g, p: rgb3(int.from_bytes(dump[pal_base + (g * 16 + p) * 2:][:2], "big"))
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    dec = {}

    def node_pixels(at):
        count, attr, sel = u(at), u(at + 2), u(at + 4)
        mixed = bool(count & 0x8000); count &= 0x7FFF
        if mixed:
            attr &= 0xE0
        pix = {}
        if not count:
            return pix
        t = TABLE_SEL + ((sel & 0x7F80) >> 5)
        table = (u(t) << 16) | u(t + 2)
        off = u(table + (sel & 127) * 2)
        geo = table + (off - 65536 if off & 0x8000 else off)
        pieces = []
        if attr & 0xFF00:
            w, h = (attr >> 8 & 15) + 1, (attr >> 12 & 15) + 1
            code = u(at + 10)
            for row in range(h):
                for col in range(w):
                    px = w - 1 - col if attr & 0x20 else col
                    py = h - 1 - row if attr & 0x40 else row
                    pieces.append(((u(geo) + px * 16) & 0xFFFF, (u(geo + 2) + py * 16) & 0xFFFF,
                                   ((code & 0xFFF0) + ((code + col) & 15) + row * 16) & 0xFFFF, attr))
        else:
            for n in range(count):
                a = u(at + 12 + n * 4) if mixed else attr
                pieces.append((u(geo + n * 4), u(geo + n * 4 + 2), u(at + 10 + n * (4 if mixed else 2)), a))
        for dx, dy, code, a in pieces:
            dx = dx - 65536 if dx & 0x8000 else dx
            dy = dy - 65536 if dy & 0x8000 else dy
            if code not in dec:
                dec[code] = decode_tile(gfx, code, 16)
            t16 = dec[code]
            for y in range(16):
                for x in range(16):
                    p = t16[15 - y if a & 0x40 else y][15 - x if a & 0x20 else x]
                    if p != 15:
                        pix[(dx + x, dy + y)] = pal(a & 31, p)
        return pix

    def chain(root, n):
        nodes, at = [], root
        for _ in range(n):
            nodes.append(at)
            count = u(at) & 0x7FFF
            size = 10 + (2 if rom[at + 2] else max(count, 1) * (4 if u(at) & 0x8000 else 2))
            nxt = at + size
            if u(at + 4) & 0x8000:
                nxt = (u(nxt) << 16) | u(nxt + 2)
            at = nxt
        return nodes

    nodes = chain(0x27222, 4) + chain(0x27318, 8) + chain(0x27716, 4)
    poses = [node_pixels(a) for a in nodes]

    # Screen pictures of the fixed arena: backdrop (plane B) and foreground (plane A).
    sw = struct.unpack(">32H", (sd / "pal.bin").read_bytes())
    spr = struct.unpack(">32H", Path(sprpal_path).read_bytes())
    cramw = list(spr) + list(sw)
    lines = [[cram(w) for w in cramw[l * 16:l * 16 + 16]] for l in range(4)]
    # The body's own colours: lines 2-3 with their cycle pens 10-15 lent to
    # the boss's fur (data/s1spr/furpal.bin, tools/sprpal.py; loaded while
    # the boss shows).
    fur = struct.unpack(">12H", (Path(sprpal_path).parent / "furpal.bin").read_bytes())
    blines = [list(l) for l in lines]
    blines[2][10:16] = [cram(w) for w in fur[:6]]
    blines[3][10:16] = [cram(w) for w in fur[6:]]
    # Hit flash (stage1_boss.md 4.4, controller 0xE6A60): each fur pen takes
    # the flash colour at the index of its nearest colour in the normal
    # palette (step 1); written as 12 CRAM words.
    flash_src = [rgb3(0xF000 | u(0xE6A60 + i * 2)) for i in range(16)]
    normal = [rgb3(0xF000 | u(0xE6A60 + 32 + i * 2)) for i in range(16)]
    def cword(c):
        return (c[0] << 1) | (c[1] << 5) | (c[2] << 9)
    flash = []
    for w in fur:
        c = cram(w)
        j = min(range(1, 15), key=lambda k: dist(c, normal[k]))
        flash.append(cword(flash_src[j]))
    (out / "furflash.bin").write_bytes(struct.pack(">12H", *flash))
    layout = (sd / "layout.h").read_text()
    pool_base = int(layout.split("STAGE_POOL_BASE ")[1].split()[0])
    pool = int(layout.split("STAGE_POOL ")[1].split()[0])
    bd_base = int(layout.split("STAGE_BD_BASE ")[1].split()[0])
    bd_tiles = (sd / "bd_tiles.bin").read_bytes()
    far_cols = int(layout.split("STAGE_FAR_COLS ")[1].split()[0]) if "STAGE_FAR_COLS" in layout else 0
    raw_bd = (sd / "bd_map.bin").read_bytes()
    bd_map = struct.unpack(">%dH" % (len(raw_bd) // 2), raw_bd)
    # The far layer (plane B, half the camera's rate): at the arena camera
    # its x is (3328 + 32) / 2 + 16 = 1696, a whole column (212).
    far_x = (3328 + 32) // 2 + 16
    assert far_x % 8 == 0
    FAR0 = far_x // 8
    fg_tiles = (sd / "tiles.bin").read_bytes()
    fg_map = (sd / "map.bin").read_bytes()
    loads = (sd / "loads.bin").read_bytes()
    # Slot -> foreground tile at the arena: replay the column loads up to the window.
    C = struct.unpack_from(">H", loads, 0)[0]
    offs = struct.unpack_from(">%dH" % (2 * C + 1), loads, 4)
    base = 4 + (2 * C + 1) * 2
    slot_tile = {}
    for c in range(min(C, CAM_COL + 42)):
        for i in range(offs[c], offs[c + 1]):
            t, sl = struct.unpack_from(">HH", loads, base + i * 4)
            slot_tile[sl] = t

    def tile_pixel(data, t, x, y, w):
        b = data[t * 32 + y * 4 + x // 2]
        return b >> 4 if x % 2 == 0 else b & 15

    def plane_pixel(word, x, y, tiles_of):
        if not word:
            return None
        tile, flip_h, flip_v, line = word & 0x7FF, word & 0x800, word & 0x1000, (word >> 13) & 3
        data, t = tiles_of(tile)
        if data is None:
            return None
        p = tile_pixel(data, t, 7 - x if flip_h else x, 7 - y if flip_v else y, 0)
        return lines[line][p] if p else None

    def bd_word(cx, cy):
        return bd_map[(FAR0 + cx) * 28 + cy] if far_cols else bd_map[cy * 40 + cx]

    def fg_word(cx, cy):
        mc, mr = CAM_COL + cx, VIEW_TOP // 8 + cy
        if mc >= C:
            return 0
        return struct.unpack_from(">H", fg_map, (mc * 64 + mr) * 2)[0]

    bd_of = lambda tile: (bd_tiles, tile - bd_base)
    fg_of = lambda tile: (fg_tiles, slot_tile.get(tile - pool_base, 0)) if tile - pool_base in slot_tile else (None, 0)

    # Region: every cell any pose touches (screen cells, 40 x 28).
    cells = set()
    for pix in poses:
        for (x, y) in pix:
            sx, sy = ORIGIN_SX + x, ORIGIN_SY + y
            if 0 <= sx < 320 and 16 <= sy < 208:
                cells.add((sx >> 3, sy >> 3))
    x0 = min(c for c, _ in cells); x1 = max(c for c, _ in cells)
    y0 = min(r for _, r in cells); y1 = max(r for _, r in cells)
    w, h = x1 - x0 + 1, y1 - y0 + 1

    blob, dirs = bytearray(), []
    preview = bytearray()
    max_t = 0
    for pix in poses:
        tiles, tid, words = [], {}, []
        img = {}
        for (x, y), c in pix.items():
            img[(ORIGIN_SX + x, ORIGIN_SY + y)] = c
        for cy in range(y0, y1 + 1):
            for cx in range(x0, x1 + 1):
                if not any((cx * 8 + x, cy * 8 + y) in img for y in range(8) for x in range(8)):
                    words.append(0xFFFF)
                    continue
                px = []
                for y in range(8):
                    row = []
                    for x in range(8):
                        sxp, syp = cx * 8 + x, cy * 8 + y
                        c = img.get((sxp, syp))
                        if c is None:
                            if plane_pixel(fg_word(cx, cy), x, y, fg_of) is not None:
                                c = None                         # the foreground shows through
                            else:
                                c = plane_pixel(bd_word(cx, cy), x, y, bd_of)
                        row.append(c)
                    px.append(row)
                opaque = [c for row in px for c in row if c is not None]
                best = min((1, 2, 3), key=lambda l: sum(min(dist(c, e) for e in blines[l][1:16]) for c in opaque))
                top = 16
                q = bytes(((0 if a is None else min(range(1, top), key=lambda j: dist(a, blines[best][j]))) << 4) |
                          (0 if b is None else min(range(1, top), key=lambda j: dist(b, blines[best][j])))
                          for row in px for a, b in zip(row[0::2], row[1::2]))
                if q not in tid:
                    tid[q] = len(tiles); tiles.append(q)
                words.append(tid[q] | (best << 13) | 0x8000)
        max_t = max(max_t, len(tiles))
        dirs.append((len(tiles), bytes(b"".join(tiles)), words))
    head = struct.pack(">HHHHH", x0, y0, w, h, len(poses))
    at = len(head) + len(dirs) * 10
    table, data = bytearray(), bytearray()
    for n, t, words in dirs:
        table += struct.pack(">HII", n, at + len(data), at + len(data) + len(t))
        data += t + b"".join(struct.pack(">H", wd) for wd in words)
    (out / "body.bin").write_bytes(head + bytes(table) + bytes(data))
    # Pool slots no foreground tile uses while the camera is at the arena
    # (columns CAM_COL-2 .. CAM_COL+43): the body's two tile buffers.
    runs = []
    starts, ends = {}, {}
    for c in range(C):
        for i in range(offs[c], offs[c + 1]):
            t, sl = struct.unpack_from(">HH", loads, base + i * 4); starts.setdefault((t, sl), []).append(c)
        for i in range(offs[C + c], offs[C + c + 1]):
            t, sl = struct.unpack_from(">HH", loads, base + i * 4); ends.setdefault((t, sl), []).append(c)
    used = set()
    for k in starts:
        for s0, e0 in zip(sorted(starts[k]), sorted(ends[k])):
            if s0 <= CAM_COL + 43 and e0 >= CAM_COL - 2:
                used.add(k[1])
    free = [pool_base + sl for sl in range(pool) if sl not in used]
    assert len(free) >= 2 * max_t, f"only {len(free)} pool slots free at the arena"
    (out / "slots.bin").write_bytes(b"".join(struct.pack(">H", v) for v in free[:2 * max_t]))
    print(f"{len(free)} pool slots free at the arena; {2 * max_t} used as the body's buffers")
    # The region's plane words for every pose in either buffer, resolved:
    # the game DMAs them as they are.
    maps = bytearray()
    for buf in (0, 1):
        sl = free[buf * max_t:(buf + 1) * max_t]
        for _, _, words in dirs:
            for i, wd in enumerate(words):
                cx, cy = x0 + i % w, y0 + i // w
                maps += struct.pack(">H", bd_word(cx, cy) if wd == 0xFFFF else (wd & 0xE000) | sl[wd & 0x1FFF])
    (out / "maps.bin").write_bytes(bytes(maps))
    (out / "layout.h").write_text(f"/* generated by tools/bosspack.py */\n#define BODY_X {x0}\n#define BODY_Y {y0}\n"
                                  f"#define BODY_W {w}\n#define BODY_H {h}\n#define BODY_POSES {len(poses)}\n"
                                  f"#define BODY_MAX_TILES {max_t}\n")
    print(f"region {w}x{h} cells at ({x0},{y0}); {len(poses)} poses, at most {max_t} tiles; "
          f"{len(head) + len(table) + len(data)} bytes")

    # Preview: each pose over the arena as shown (backdrop + body cells; foreground left out).
    W, H = 320, 224
    img_all = bytearray()
    for n, t, words in dirs:
        im = bytearray(W * H * 3)
        for cy in range(28):
            for cx in range(40):
                inside = x0 <= cx <= x1 and y0 <= cy <= y1
                wd = words[(cy - y0) * w + (cx - x0)] if inside else 0xFFFF
                for y in range(8):
                    for x in range(8):
                        if wd == 0xFFFF:
                            c = plane_pixel(bd_word(cx, cy), x, y, bd_of)
                            f = plane_pixel(fg_word(cx, cy), x, y, fg_of)
                            c = f if f is not None else c
                        else:
                            p = tile_pixel(t, wd & 0x1FFF, x, y, 0)
                            c = blines[(wd >> 13) & 3][p] if p else plane_pixel(fg_word(cx, cy), x, y, fg_of)
                        k = ((cy * 8 + y) * W + cx * 8 + x) * 3
                        im[k:k + 3] = bytes(v * 36 for v in c) if c else b"\0\0\0"
        img_all += im
    (out / "preview.ppm").write_bytes(b"P6\n%d %d\n255\n" % (W, H * len(dirs)) + bytes(img_all))


if __name__ == "__main__":
    main()
