#!/usr/bin/env python3
"""posepack.py PALDUMP.cps OUTDIR [ROOT_TABLE...]: Willow's 70 animation poses as ready
Genesis sprites (NATIVE_ENGINE_PLAN.md, M1).

Each arcade pose (an animation node: a list of 16x16 pieces, ROM data read
offline) is drawn into a canvas around the player origin, then cut into
Genesis sprites of up to 32x32 covering only non-empty 8x8 cells. At run
time a pose is one DMA of its tiles (when the pose changes) and a copy of
its sprite entries with the player position added.

Pose node addresses: build/native-world/player-hold-data-v1/manifest.json
(native pose index = node index, specs/player.md section 5). Colours: the
pose's CPS sprite palette group from the dump, one Genesis line (line 0).

Writes (big-endian):
  pal.bin     16 u16 CRAM words (entry 0 transparent)
  poses.bin   70 x {u16 tile offset (in tiles), u8 tile count, u8 sprite
              count, u16 sprite list offset (in entries)}; then sprite
              entries {s16 dy, u8 size (VDP: (w-1)<<2 | (h-1); bit 7: H-flip,
              with MIRROR=1 a mirrored pose shares an earlier pose's tiles), u8 tile
              index within the pose, s16 dx} relative to the origin
              (screen y down)
  tiles.bin   all poses' tiles, 32 bytes each, column-major per sprite
  nodes.bin   (Willow) per pose: u16 ticks (FFFF: hold), u8 next pose, u8 0
  preview.ppm all poses on a grid
  boxes.bin   (BOXES=base,...) per pose, per geometry table: 4 s16
              (xOffset, width, yOffset, height)

With ROOT_TABLE addresses (hex; "ADDR:N" for a table of N entries, default
2 = one per facing; "rADDR" for a single chain starting at node ADDR;
"hADDR" for a single pose held by code, "HADDR[:N]" for a table whose
entries are poses held by code), the poses are instead every animation node
reachable from those root tables (each: a word offset per facing to the
first node; a node's successor follows it, or a long pointer after it when
its selector word is negative: arcade routine 1F30/1F4C), and anim.bin
lists per (table, facing) the chain: u8 count, then (u8 pose, u8 ticks)...
with the loop target index as a final u8 (FF: holds on the last).
"""
import json, os, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

TABLE_SEL = 0x3B9B2


def chan(v):
    return min(7, (v + 16) >> 5)


def rgb3(w):
    bright = 0x0F + ((w >> 12) << 1)
    return tuple(chan(((w >> s) & 15) * 0x11 * bright // 0x2D) for s in (8, 4, 0))


def main():
    dump_path, outdir = sys.argv[1:3]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    u = lambda a: struct.unpack_from(">H", rom, a)[0]
    meta = json.loads((ROOT / "build/native-world/player-hold-data-v1/manifest.json").read_text())
    first = int(os.environ.get("NODE_FIRST", "0"))          # 0: Willow; 70: Madmartigan (nodes 70-139, own palette)
    nodes = [(n["source"], None) for n in meta["nodes"]][first:first + 70]
    def valid(at):
        """A plausible animation node at at (a held pose's next pointer runs
        into unrelated data)."""
        if at & 1 or not 0 < at < len(rom) - 16:
            return False
        count, attr, ticks = u(at) & 0x7FFF, u(at + 2), u(at + 8)
        return 0 < count <= 40 and (attr & 0x1F) < 32 and (ticks == 0xFFFF or ticks < 512)

    chains = []
    if len(sys.argv) > 3:
        nodes = []
        for arg in sys.argv[3:]:
            # "rADDR": one chain from the node at ADDR; otherwise a root table
            # with a word offset per facing. A "@G" suffix draws that root's
            # poses with CPS palette group G (a code-set colour override).
            arg, _, gov = arg.partition("@")
            gov = int(gov) if gov else None
            held = arg[0] in "hH"
            if arg[0] in "rh":
                starts = [int(arg[1:], 16)]
            else:
                tab, _, n = arg.lstrip("H").partition(":")
                tab = int(tab, 16)
                starts = [tab + (u(tab + f * 2) - (0x10000 if u(tab + f * 2) & 0x8000 else 0)) for f in range(int(n or 2))]
            for at in starts:
                if held:                              # one pose, held by code: never follow its next
                    if (at, gov) not in nodes:
                        nodes.append((at, gov))
                    chains.append(bytes([1, nodes.index((at, gov)), 255, 0xFF]))
                    continue
                chain, seen = [], {}
                while at not in seen:
                    seen[at] = len(chain)
                    if (at, gov) not in nodes:
                        nodes.append((at, gov))
                    chain.append((nodes.index((at, gov)), u(at + 8)))
                    count, sel = u(at) & 0x7FFF, u(at + 4)
                    size = 10 + (2 if rom[at + 2] else max(count, 1) * (4 if u(at) & 0x8000 else 2))   # an empty node keeps one piece slot
                    if u(at + 8) == 0xFFFF:
                        seen[at] = None
                        break
                    nxt = at + size
                    if sel & 0x8000:
                        nxt = (u(nxt) << 16) | u(nxt + 2)
                    if not valid(nxt):                # runs into other data: the code holds this pose
                        seen[at] = None
                        break
                    at = nxt
                loop = seen.get(at)
                chains.append(bytes([len(chain)]) + b"".join(bytes([p, min(t, 255)]) for p, t in chain) +
                              bytes([0xFF if loop is None else loop]))
    raw = Path(dump_path).read_bytes()
    dump = raw[0x80:]
    pal_base = (int.from_bytes(raw[0x0A:0x0C], "big") << 8) - 0x900000
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    dec = {}

    poses = []                                   # per pose: {(x, y): pen} screen-down, origin 0,0
    groups = set()
    for at, gov in nodes:
        count, attr, sel = u(at), u(at + 2), u(at + 4)
        mixed = bool(count & 0x8000); count &= 0x7FFF
        if mixed:
            attr &= 0xE0
        pix = {}
        if count:
            t = TABLE_SEL + ((sel & 0x7F80) >> 5)
            table = (u(t) << 16) | u(t + 2)
            off = u(table + (sel & 127) * 2)
            off = off - 65536 if off & 0x8000 else off
            geo = table + off
            packed = bool(attr & 0xFF00)
            pieces = []
            if packed:
                w, h = (attr >> 8 & 15) + 1, (attr >> 12 & 15) + 1
                code = u(at + 10)
                for row in range(h):
                    for col in range(w):
                        # CPS-1 draws a flipped block with its tiles in reverse order.
                        px = w - 1 - col if attr & 0x20 else col
                        py = h - 1 - row if attr & 0x40 else row
                        pieces.append(((u(geo) + px * 16) & 0xFFFF, (u(geo + 2) + py * 16) & 0xFFFF,
                                       ((code & 0xFFF0) + ((code + col) & 15) + row * 16) & 0xFFFF, attr))
            else:
                for n in range(count):
                    a = u(at + 12 + n * 4) if mixed else attr
                    # A flipped node mirrors its pieces' positions about the
                    # origin as well as their pixels (x -> -x - 16).
                    gx, gy = u(geo + n * 4), u(geo + n * 4 + 2)
                    gx = gx - 65536 if gx & 0x8000 else gx
                    gy = gy - 65536 if gy & 0x8000 else gy
                    if attr & 0x20: gx = -gx - 16
                    if attr & 0x40: gy = -gy - 16
                    pieces.append((gx & 0xFFFF, gy & 0xFFFF, u(at + 10 + n * (4 if mixed else 2)), a))
            for dx, dy, code, a in pieces:
                dx = dx - 65536 if dx & 0x8000 else dx
                dy = dy - 65536 if dy & 0x8000 else dy
                if code not in dec:
                    dec[code] = decode_tile(gfx, code, 16)
                t16 = dec[code]
                g = a & 31 if gov is None else gov
                groups.add(g)
                for y in range(16):
                    for x in range(16):
                        p = t16[15 - y if a & 0x40 else y][15 - x if a & 0x20 else x]
                        if p != 15:
                            pix[(dx + x, dy + y)] = (g, p)
        poses.append(pix)
    colour = lambda g, p: rgb3(int.from_bytes(dump[pal_base + (g * 16 + p) * 2:][:2], "big"))
    if os.environ.get("PIXOUT"):                # the poses' pixels as RGB (screen down, origin 0,0): tools/neckpack5.py
        import pickle
        pickle.dump([{k: colour(*v) for k, v in pix.items()} for pix in poses], open(os.environ["PIXOUT"], "wb"))
    if len(groups) == 1:
        g = groups.pop()
        line = [colour(g, p) for p in range(15)]
        index = {(g, p): p for p in range(15)}
        print(f"{len(poses)} poses, sprite palette group {g}")
    else:
        # Several CPS palette groups (a composite such as the wagon's horse):
        # one virtual 15-colour palette over the colours they use (clustered
        # when more); tools/sprpal.py later puts each piece on a line anyway.
        used = {}
        for pix in poses:
            for v in pix.values():
                used[v] = used.get(v, 0) + 1
        cols = {}
        for (g, p), n in used.items():
            c = colour(g, p); cols[c] = cols.get(c, 0) + n
        if len(cols) <= 15:
            line = list(cols)
        else:
            sys.path.insert(0, str(Path(__file__).resolve().parent))
            from intropack import kmeans
            line = kmeans(cols, 15)
        line += [(0, 0, 0)] * (15 - len(line))
        dd = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b))
        index = {v: min(range(15), key=lambda i: dd(colour(*v), line[i])) for v in used}
        print(f"{len(poses)} poses, sprite palette groups {sorted(groups)} -> {len(cols)} colours")
    poses = [{k: index[v] for k, v in pix.items()} for pix in poses]

    pose_dir, ents, tiles = bytearray(), bytearray(), bytearray()
    nent = ntile = 0
    max_spr = max_t = 0
    seen_blocks = {}
    mirror = os.environ.get("MIRROR") == "1"
    SPR_COST = int(os.environ.get("SPR_COST", "3"))     # tiles a sprite piece must save (more: fewer, larger pieces)
    packed = []                                 # (pix, first tile, tiles, entries) of poses with own tiles
    for pix in poses:
        first_t, first_e = ntile, nent
        nspr = 0
        pose_ents = []
        if mirror and pix:
            # A horizontal mirror of an earlier pose reuses its tiles with the
            # sprites' H-flip (size byte bit 7): x -> c - x.
            hit = None
            for q, qt, qn, qe in packed:
                if len(q) != len(pix):
                    continue
                c = min(x for x, _ in q) + max(x for x, _ in pix)
                if all(q.get((c - x, y)) == v for (x, y), v in pix.items()):
                    hit = (qt, qn, qe, c)
                    break
            if hit:
                qt, qn, qe, c = hit
                for dy, size, t, dx in qe:
                    w = (size >> 2) + 1
                    ents += struct.pack(">hBBh", dy, size | 0x80, t, c - dx - 8 * w + 1)
                    nent += 1
                pose_dir += struct.pack(">HBBH", qt, qn, len(qe), first_e)
                continue
        if pix:
            # Cut: try every pixel offset of the 8-px cell grid and every
            # cell offset of the 32-px block grid; keep the fewest tiles,
            # then the fewest sprites.
            xs = [x for x, _ in pix]; ys = [y for _, y in pix]
            best = None
            for sx in range(8):
                for sy in range(8):
                    x0, y0 = min(xs) - sx, min(ys) - sy
                    cells = {((x - x0) >> 3, (y - y0) >> 3) for x, y in pix}
                    for ox in range(4):
                        for oy in range(4):
                            blocks = {}
                            for c, r in cells:
                                blocks.setdefault(((c + ox) >> 2, (r + oy) >> 2), []).append((c, r))
                            cut, nt = [], 0
                            for inb in blocks.values():
                                c0 = min(c for c, _ in inb); c1 = max(c for c, _ in inb)
                                r0 = min(r for _, r in inb); r1 = max(r for _, r in inb)
                                cut.append((c0, c1, r0, r1))
                                nt += (c1 - c0 + 1) * (r1 - r0 + 1)
                            key = (nt + SPR_COST * len(cut), len(cut))   # a sprite must save SPR_COST tiles (sprite limits, CPU)
                            if best is None or key < best[0]:
                                best = (key, x0, y0, sorted(cut, key=lambda b: (b[2], b[0])))
            _, x0, y0, cut = best
            for c0, c1, r0, r1 in cut:
                    w, h = c1 - c0 + 1, r1 - r0 + 1
                    ent = (y0 + r0 * 8, ((w - 1) << 2) | (h - 1), ntile - first_t, x0 + c0 * 8)
                    ents += struct.pack(">hBBh", *ent)
                    pose_ents.append(ent)
                    nent += 1; nspr += 1
                    for c in range(c0, c1 + 1):          # VDP sprite tiles: column-major
                        for r in range(r0, r1 + 1):
                            for y in range(8):
                                for x in range(0, 8, 2):
                                    px = [pix.get((x0 + c * 8 + x + k, y0 + r * 8 + y)) for k in (0, 1)]
                                    tiles.append(((px[0] + 1 if px[0] is not None else 0) << 4) |
                                                 (px[1] + 1 if px[1] is not None else 0))
                            ntile += 1
        max_spr = max(max_spr, nspr); max_t = max(max_t, ntile - first_t)
        block = bytes(tiles[first_t * 32:])
        if block and block in seen_blocks:            # same tiles as an earlier pose: share them
            del tiles[first_t * 32:]
            n_t = ntile - first_t
            ntile, first_t = first_t, seen_blocks[block]
            pose_dir += struct.pack(">HBBH", first_t, n_t, nspr, first_e)
            continue
        if block:
            seen_blocks[block] = first_t
            packed.append((pix, first_t, ntile - first_t, pose_ents))
        pose_dir += struct.pack(">HBBH", first_t, ntile - first_t, nspr, first_e)
    (out / "poses.bin").write_bytes(bytes(pose_dir) + bytes(ents))
    # Hit boxes: BOXES=base,base... (combat.md 1.2): per pose and base, the
    # node's geometry entry (node word +6, signed, * 8 + base): 4 s16
    # (xOffset, width, yOffset, height).
    if os.environ.get("BOXES"):
        bases = [int(b, 16) for b in os.environ["BOXES"].split(",")]
        bx = bytearray()
        for at, _ in nodes:
            g = u(at + 6)
            g = g - 65536 if g & 0x8000 else g
            for b in bases:
                bx += rom[b + g * 8:b + g * 8 + 8]
        (out / "boxes.bin").write_bytes(bytes(bx))
    (out / "tiles.bin").write_bytes(bytes(tiles))
    palw = struct.pack(">H", 0) + b"".join(struct.pack(">H", (r << 1) | (g_ << 5) | (b << 9)) for r, g_, b in line)
    (out / "pal.bin").write_bytes(palw)
    P = os.environ.get("PREFIX", "POSE")
    if chains:
        (out / "anim.bin").write_bytes(b"".join(chains))
    else:                                     # Willow: the node graph (u16 ticks, u8 next, u8 0)
        (out / "nodes.bin").write_bytes(b"".join(
            struct.pack(">HBB", n["ticks"], n["next"] - first if first <= n["next"] < first + 70 else i, 0)
            for i, n in enumerate(meta["nodes"][first:first + len(poses)])))
    (out / "layout.h").write_text("/* generated by tools/posepack.py */\n"
                                  f"#define {P}_COUNT {len(poses)}\n#define {P}_TILES {ntile}\n#define {P}_MAX_TILES {max_t}\n"
                                  f"#define {P}_MAX_SPRITES {max_spr}\n")
    print(f"{ntile} tiles ({len(tiles)} bytes), {nent} sprites; per pose at most {max_t} tiles, {max_spr} sprites")

    # Preview: 14 x 10 grid of 96x96 cells, origin at (48, 72).
    W, H = 14 * 96, 10 * 96
    img = bytearray(W * H * 3)
    for i, pix in enumerate(poses):
        ox, oy = (i % 14) * 96 + 48, (i // 14) * 96 + 72
        for (x, y), p in pix.items():
            if 0 <= x + 48 < 96 and 0 <= y + 72 < 96:
                k = ((oy + y) * W + ox + x) * 3
                img[k:k + 3] = bytes(v * 36 for v in line[p])
    (out / "preview.ppm").write_bytes(b"P6\n%d %d\n255\n" % (W, H) + bytes(img))


if __name__ == "__main__":
    main()
