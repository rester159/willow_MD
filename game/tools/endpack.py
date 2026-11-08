#!/usr/bin/env python3
"""endpack.py RDIR OUTDIR: the ending's castle scene and closing card
(specs/stage6.md 7.1 subs 14-38) from run R's dumps (RDIR = S/vidR).

Castle scene: the castle is scroll2 palette 14 alone and the sky with the
hills scroll3 palette 12 alone, so each becomes one CRAM line with the
arcade's pens kept (pen p -> Genesis entry p + 1, pen 15 transparent): the
castle on plane A (line 2, drawn from the scene's first frame, scroll y 512),
the sky on plane B (line 3). The sunset brightening to day is then only CRAM:
end_seq.bin lists the distinct states of both palettes through the scene
(u16 update offset from sub 16, 32 CRAM words), from the dumps every 6 keys.
The closing card (Willow's portrait and the logos, v009600) is one picture
fitted to all four lines (tools/intropack.py picture).

Writes end_castle_{tiles,map}.bin (40 x 32 words), end_sky_{tiles,map}.bin
(40 x 28), end_seq.bin, end_card_{tiles,map,pal}.bin and end.h."""
import os, struct, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import intropack as ip  # noqa: E402
c = ip.cpsframe
cps_rgb = c.cps_rgb

SUB16 = 7540                     # run R: the scene's sub 16 (the sky starts to change)


def sky_layer(dump):
    """scroll3 at its scroll, every tile code (cpsframe keeps only the stages' 0xA00..0xDFF)."""
    d = c.Dump(dump)
    base = (d.reg(0x800106) << 8) & 0xFFFFFF
    sx, sy = d.reg(0x800114), d.reg(0x800116)
    out = {}
    for y in range(224):
        yy = (y + c.TOP + sy) % 2048; row = yy // 32
        for x in range(320):
            xx = (x + c.LEFT + sx) % 2048; col = xx // 32
            a = base + ((row & 7) + (col << 3) + ((row & 56) << 6)) * 4
            code, attr = d.word(a) & 0x3FFF, d.word(a + 2)
            t = c.tile(code, 32)
            px, py = xx % 32, yy % 32
            pen = t[31 - py if attr & 64 else py][31 - px if attr & 32 else px]
            if pen != 15: out[x, y] = (0x600 + (attr & 31) * 16 + pen, 0, pen)
    return d, out


def layer(dump, n, sy=None):
    d = c.Dump(dump)
    if sy is not None:
        o = {2: 0x800112, 3: 0x800116}[n] - 0x800100
        d.regs = d.regs[:o] + struct.pack(">H", sy) + d.regs[o + 2:]
    return d, c.scroll_layer(d, n)


def pen_tiles(px, rows, line, base, pri=0):
    """px {(x, y): pen 0..14}: 8x8 tiles (pen + 1), flip-deduplicated; words column-major? no: row-major 40 wide."""
    tiles, index, words = [], {}, []
    for cy in range(rows):
        for cx in range(40):
            t = tuple(px[(cx * 8 + x, cy * 8 + y)] + 1 if (cx * 8 + x, cy * 8 + y) in px else 0 for y in range(8) for x in range(8))
            if not any(t):
                words.append(0); continue
            for f in range(4):
                k = tuple(ip.flip(list(t), f))
                if k in index:
                    words.append((base + index[k]) | (f << 11) | (line << 13) | pri); break
            else:
                index[t] = len(tiles); tiles.append(list(t)); words.append((base + index[t]) | (line << 13) | pri)
    return tiles, words


def main():
    rdir, outdir = Path(sys.argv[1]), Path(sys.argv[2])
    out = outdir; out.mkdir(parents=True, exist_ok=True)
    first = str(rdir / "v007542.cps")
    # Castle (scroll2, palette 14) at its first position; sky (scroll3, palette 12).
    d, cas = layer(first, 2, 512)
    castle = {xy: v[2] for xy, v in cas.items()}
    assert all((v[0] - 0x400) // 16 == 14 for v in cas.values())
    _, sk = sky_layer(first)
    sky = {xy: v[2] for xy, v in sk.items()}
    assert all((v[0] - 0x600) // 16 == 12 for v in sk.values())
    # The hills: scroll1 palette 17 over the castle (layer order 0x12F8): plane B
    # tiles on line 0 with priority; a hill cell's sky pixels take the nearest
    # hill colour of the sunset palette.
    _, hl = layer(first, 1)
    hills = {xy: v[2] for xy, v in hl.items() if (v[0] - 0x200) // 16 == 17}
    dd = c.Dump(first)
    rgbw = lambda page, grp, p: ip.q3(cps_rgb(dd.word(0x91C000 + 2 * (page + grp * 16 + p))))
    hcol = [rgbw(0x200, 17, p) for p in range(15)]
    for cy in range(28):
        for cx in range(40):
            cell = [(cx * 8 + x, cy * 8 + y) for y in range(8) for x in range(8)]
            if not any(q in hills for q in cell): continue
            for q in cell:
                if q not in hills and q in sky:
                    sc = rgbw(0x600, 12, sky[q])
                    hills[q] = min(range(15), key=lambda j: ip.dist(sc, hcol[j]))
                sky.pop(q, None)
    st, sw = pen_tiles(sky, 28, 3, 16)
    ht, hw = pen_tiles(hills, 28, 0, 16 + len(st), 0x8000)
    sw = [h if h else w for w, h in zip(sw, hw)]
    st = st + ht
    ct, cw = pen_tiles(castle, 32, 2, 16 + len(st))
    print(f"castle scene: sky {len(st)} tiles, castle {len(ct)} tiles; castle rows {min(y for x, y in castle)}..{max(y for x, y in castle)}")
    (out / "end_sky_tiles.bin").write_bytes(b"".join(ip.pack_tile(t) for t in st))
    (out / "end_sky_map.bin").write_bytes(struct.pack(">%dH" % len(sw), *sw))
    (out / "end_castle_tiles.bin").write_bytes(b"".join(ip.pack_tile(t) for t in ct))
    (out / "end_castle_map.bin").write_bytes(struct.pack(">%dH" % len(cw), *cw))
    # Palette states (lines 2, 3) through the scene.
    def line_words(dd, page, grp):
        return [0] + [ip.cram_word(ip.q3(cps_rgb(dd.word(0x91C000 + 2 * (page + grp * 16 + p))))) for p in range(15)]
    seq, last = [], None
    for k in range(SUB16 + 2, SUB16 + 424, 6):           # the dumps: every 6 keys from 7002
        f = rdir / ("v%06d.cps" % k)
        if not f.exists(): continue
        dd = c.Dump(str(f))
        w = line_words(dd, 0x200, 17) + line_words(dd, 0x400, 14) + line_words(dd, 0x600, 12)
        if w != last:
            seq.append((max(0, k - SUB16), w)); last = w
    (out / "end_seq.bin").write_bytes(struct.pack(">H", len(seq)) + b"".join(struct.pack(">H48H", t, *w) for t, w in seq))
    print(f"palette states: {len(seq)} ({[t for t, _ in seq]})")
    # The closing card: the full frame (portrait, logos), four lines fitted.
    os.environ.pop("CPSPLANE23", None)
    rgb = c.compose(c.Dump(str(rdir / "v009600.cps")))
    img = {(x, y): ip.q3(rgb[(y * 320 + x) * 3:(y * 320 + x) * 3 + 3]) for y in range(224) for x in range(320)}
    lines = [[None] * 16 for _ in range(4)]
    tiles, words, n = ip.picture(img, 320, 224, lines, {l: list(range(1, 16)) for l in range(4)}, ip.MAP_BUDGET)
    (out / "end_card_tiles.bin").write_bytes(tiles)
    (out / "end_card_map.bin").write_bytes(struct.pack(">1120H", *words))
    (out / "end_card_pal.bin").write_bytes(struct.pack(">64H", *[ip.cram_word(cc) if cc else 0 for l in lines for cc in l]))
    print(f"closing card: {n} tiles")
    (out / "end.h").write_text("/* generated by tools/endpack.py */\n"
                               f"#define END_SKY_TILES {len(st)}\n#define END_CASTLE_TILES {len(ct)}\n#define END_SEQ_STATES {len(seq)}\n#define END_CARD_TILES {n}\n")


if __name__ == "__main__":
    main()
