#!/usr/bin/env python3
"""hudpack.py STAGE_PAL OUTDIR [SPRITE_PAL...]: the stage HUD's glyphs as
ready Genesis tiles (NATIVE_ENGINE_PLAN.md M1, specs/hud.md, approved cut 2).

The arcade draws its HUD on the text layer with glyphs 0x7000-0x77FF and
the 32 fixed text palettes (ROM 0xE6EA0). Here every glyph the HUD can show
is decoded once and coloured offline for the stage's fixed CRAM (cut 4):
each glyph group (one arcade palette) takes the CRAM line that reproduces
it best, and every pen the nearest colour in that line. The game only
writes plane words pointing at these tiles.

CRAM lines: 0 Willow (SPRITE_PAL 1), 1 shot (SPRITE_PAL 2), 2-3 scenery
(STAGE_PAL; STATIC=9 when its pens 10-15 cycle). Writes (big-endian):
  tiles.bin    resident HUD tiles, 32 bytes each
  partial.bin  charge bar partial-fill tiles, [palette 0x11..0x1D][fill
               0..6][top, bottom], 32 bytes each (streamed into one slot pair)
  partline.bin u8 CRAM line per partial palette (13)
  hud_tiles.h  tile numbers (offset from the HUD base) | line << 13
"""
import os, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

TEXT_PALS = 0xE6EA0


def chan(v):
    return min(7, (v + 16) >> 5)


def rgb3(w):
    bright = 0x0F + ((w >> 12) << 1)
    return tuple(chan(((w >> s) & 15) * 0x11 * bright // 0x2D) for s in (8, 4, 0))


def cram(w):
    return ((w >> 1) & 7, (w >> 5) & 7, (w >> 9) & 7)


def main():
    stage_pal, outdir = sys.argv[1:3]
    out = Path(outdir); out.mkdir(parents=True, exist_ok=True)
    words = []
    for f in sys.argv[3:] + [stage_pal]:
        d = Path(f).read_bytes()
        words += struct.unpack(">%dH" % (len(d) // 2), d)
    lines = [[cram(w) for w in words[i:i + 16]] for i in range(0, 64, 16)]
    assert len(lines) == 4
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    apal = [[rgb3(0xF000 | struct.unpack_from(">H", rom, TEXT_PALS + (p * 16 + i) * 2)[0])
             for i in range(16)] for p in range(32)]
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)

    static = int(os.environ.get("STATIC", "15"))   # scenery lines: pens above this cycle (stagepack CYCLE)

    def nearest(c, line):
        top = 16 if line is lines[0] or line is lines[1] else static + 1
        return min(range(1, top), key=lambda i: sum((a - b) ** 2 for a, b in zip(c, line[i])))

    def err(codes, p, line):
        e = 0
        for code in codes:
            for row in decode_tile(gfx, code, 8):
                for pen in row:
                    if pen != 15:
                        c = apal[p][pen]
                        e += sum((a - b) ** 2 for a, b in zip(c, line[nearest(c, line)]))
        return e

    # LINES_FROM=another stage's hud_tiles.h: keep its line per group (the game
    # is compiled with one set of HT_ constants; only the colours change).
    keep = {}
    if os.environ.get("LINES_FROM"):
        for ln in Path(os.environ["LINES_FROM"]).read_text().split("\n"):
            f = ln.split()
            if len(f) == 3 and f[0] == "#define" and f[1].startswith("HT_") and f[2].startswith("0x"):
                keep[f[1]] = int(f[2], 16) >> 13

    def best_line(codes, p, name=None):       # never line 0: Willow's, it flashes when she is hurt
        if name is not None and keep:
            return keep[name]
        return min(range(1, 4), key=lambda l: err(codes, p, lines[l]))

    def tile(code, p, l):
        t = bytearray()
        px = decode_tile(gfx, code, 8)
        for row in px:
            for x in range(0, 8, 2):
                a, b = (0 if pen == 15 else nearest(apal[p][pen], lines[l]) for pen in row[x:x + 2])
                t.append(a << 4 | b)
        return bytes(t)

    tiles, names, report = bytearray(), [], []

    def group(name, p, codes):
        """Pack codes (list of (suffix, code)) coloured with arcade palette p."""
        l = best_line([c for _, c in codes], p, f"{name}_{codes[0][0]}")
        report.append(f"{name}: arcade palette {p:#x} -> line {l}, "
                      f"error {err([c for _, c in codes], p, lines[l])}")
        for suffix, code in codes:
            names.append((f"{name}_{suffix}", len(tiles) // 32, l))
            tiles.extend(tile(code, p, l))
        return l

    digits = [(str(d), 0x7000 + d) for d in range(10)]
    group("HT_BLUE", 0, digits + [(n, 0x7000 + ord(ch)) for n, ch in
                                  (("U", "U"), ("P", "P"), ("H", "H"), ("I", "I"), ("V", "V"),
                                   ("T", "T"), ("A", "A"), ("L", "L"), ("EQ", "="))])
    group("HT_MSG", 0, [(ch, 0x7000 + ord(ch)) for ch in "MEORCNGSKBWYDFJQXZ"] +
          [("DOT", 0x702E), ("COMMA", 0x702C), ("APOS", 0x7027), ("EXCL", 0x7021), ("QUES", 0x703F), ("DASH", 0x702D)])   # message letters (TIME OVER, CONTINUE, GAME OVER, STAGE CLEAR ...)
    group("HT_YEL", 2, digits + [(ch, 0x7000 + ord(ch)) for ch in "GOLD"])
    group("HT_RED", 3, digits + [(ch, 0x7000 + ord(ch)) for ch in "TIME"] + [("COLON", 0x703A)])
    group("HT_GEM", 0x10, [("FULL", 0x7784), ("EMPTY", 0x7783)])
    group("HT_ARMOR", 0x10, [("CELL", 0x7793)])
    group("HT_BOSS", 0x10, [("CELL", 0x7794)])
    group("HT_BAND", 0, [("BLANK", 0x7120)])
    group("HT_CAP", 0x11, [("LT", 0x7780), ("LB", 0x7790), ("RT", 0x7782), ("RB", 0x7792)])
    # Charge bar colours: a magic level uses groups level .. level + 6 (text
    # palettes 0x11 + level ..). Groups 0-6 (level 0) come first so that
    # level 0 loads only the tiles up to HT_RESIDENT; the rest (higher
    # levels, item icons) follow and are loaded when needed.
    def bars(groups):
        for p in groups:
            group(f"HT_BAR{p - 0x11:X}", p, [("ET", 0x7781), ("EB", 0x7791), ("FT", 0x77A7), ("FB", 0x77B7)])
    bars(range(0x11, 0x18))
    first_partial_slot = len(tiles) // 32
    names.append(("HT_PART_T", first_partial_slot, 0))
    names.append(("HT_PART_B", first_partial_slot + 1, 0))
    tiles.extend(bytes(64))
    names.append(("HT_RESIDENT", len(tiles) // 32, 0))
    bars(range(0x18, 0x1E))
    icons = [(0x14, 0x7786), (0x14, 0x7788), (0x14, 0x778A), (0x13, 0x77A8), (0x13, 0x778C), (0x13, 0x77AA)]
    for n, (p, c) in enumerate(icons):          # top-left, bottom-left, top-right, bottom-right
        group(f"HT_ICON{n}", p, [("TL", c), ("BL", c + 0x10), ("TR", c + 1), ("BR", c + 0x11)])
    # Madmartigan's sword icon (5040; table 0x4C8A + level * 16, palette
    # 0x4C84[level]): 4 x 2 glyphs per sword level, column by column (top,
    # bottom); 8 tiles streamed after the item icons when the level changes.
    for lv, p in enumerate((0x11, 0x11, 0x11, 0x12, 0x12)):
        c = (0x77C0, 0x77C4, 0x77C8, 0x77CC, 0x77E0)[lv]
        group(f"HT_SWORD{lv}", p, [(f"{k}{tb}", c + k + (0x10 if tb == "B" else 0)) for k in range(4) for tb in "TB"])

    partial, pline = bytearray(), bytearray()
    for p in range(0x11, 0x1E):
        codes = [0x77A0 + f for f in range(7)] + [0x77B0 + f for f in range(7)]
        l = best_line(codes, p)
        pline.append(l)
        for f in range(7):
            partial += tile(0x77A0 + f, p, l) + tile(0x77B0 + f, p, l)

    (out / "tiles.bin").write_bytes(bytes(tiles))
    (out / "partial.bin").write_bytes(bytes(partial))
    (out / "partline.bin").write_bytes(bytes(pline) + bytes(len(pline) & 1))
    h = ["/* generated by tools/hudpack.py: tile offset from the HUD base | CRAM line << 13 */",
         f"#define HT_COUNT {len(tiles) // 32}"]
    h += [f"#define {n} {i | l << 13:#06x}" for n, i, l in names]
    (out / "hud_tiles.h").write_text("\n".join(h) + "\n")
    print("\n".join(report))
    print(f"{len(tiles) // 32} resident tiles, {len(partial) // 32} partial-fill tiles")


if __name__ == "__main__":
    main()
