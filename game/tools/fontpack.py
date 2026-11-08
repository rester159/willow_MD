#!/usr/bin/env python3
"""fontpack.py OUTDIR: the arcade's text font (scroll1 glyphs 0x7000 + ASCII
32..95, the HUD's and menus' letters) as 64 Genesis tiles with the arcade
pens kept (pen 15 transparent -> 0, pen p -> p + 1), and two CRAM lines from
the arcade's fixed text palettes (ROM 0xE6EA0): 0 normal, 2 highlight
(the HUD's blue-white and yellow). For the port's menus (src/frontend.c).
Writes font.bin (64 tiles) and pal.bin (32 CRAM words: line normal, line
highlight)."""
import struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from cps1_gfx import assemble_region, decode_tile  # noqa: E402

TEXT_PALS = 0xE6EA0


def rgb3(w):
    bright = 0x0F + ((w >> 12) << 1)
    return tuple(min(7, (((w >> s) & 15) * 0x11 * bright // 0x2D + 16) >> 5) for s in (8, 4, 0))


def main():
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
    rom = (ROOT / "build/reference_maincpu.bin").read_bytes()
    with zipfile.ZipFile(ROOT / "willow.zip") as z:
        gfx = assemble_region(z)
    tiles = bytearray()
    for ch in list(range(32, 96)) + [0x7B, 0x7C]:     # + the name entry's RB and ED glyphs (glyphs 64, 65)
        t = decode_tile(gfx, 0x7000 + ch, 8)
        for row in t:
            for x in range(0, 8, 2):
                a, b = (0 if p == 15 else p + 1 for p in row[x:x + 2])
                tiles.append(a << 4 | b)
    (out / "font.bin").write_bytes(bytes(tiles))
    words = []
    for p in (0, 2):
        line = [0]
        for i in range(15):
            r, g, b = rgb3(0xF000 | struct.unpack_from(">H", rom, TEXT_PALS + (p * 16 + i) * 2)[0])
            line.append((r << 1) | (g << 5) | (b << 9))
        words += line
    (out / "pal.bin").write_bytes(struct.pack(">32H", *words))
    big = bytearray()
    codes = [0x7680 + (n % 8) * 2 + (n // 8) * 0x20 for n in range(26)]
    codes += [0x7B22] + [0x7B00 + 2 * (n - 1) for n in range(1, 9)] + [0x7B20]
    for c in codes:
        for sub in (c, c + 0x10, c + 1, c + 0x11):        # VDP column-major: TL, BL, TR, BR
            for row in decode_tile(gfx, sub, 8):
                for x in range(0, 8, 2):
                    a, b = (0 if p == 15 else p + 1 for p in row[x:x + 2])
                    big.append(a << 4 | b)
    (out / "big.bin").write_bytes(bytes(big))
    allp = []
    for p in range(32):
        allp.append(0)
        for i in range(15):
            r, g, b = rgb3(0xF000 | struct.unpack_from(">H", rom, TEXT_PALS + (p * 16 + i) * 2)[0])
            allp.append((r << 1) | (g << 5) | (b << 9))
    (out / "textpal.bin").write_bytes(struct.pack(">512H", *allp))
    print("font: 64 glyphs, 2 palettes")


if __name__ == "__main__":
    main()
