#!/usr/bin/env python3
"""titleglyphs.py CHAR=CODE...: append big title glyphs (2x2 cells of the
arcade's big font, text palette 0x0E, intropack.py's colouring) to
data/intro/font.bin and their indices to font_title in intro_text.h, for the
later stages' titles (stage3.md 1.3, stage4.md 1.4). Glyphs already there
are kept."""
import re, struct, sys, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import intropack as ip  # noqa: E402

rom = (ip.ROOT / "build/reference_maincpu.bin").read_bytes()
with zipfile.ZipFile(ip.ROOT / "willow.zip") as z:
    gfx = ip.assemble_region(z)
sprpal = struct.unpack(">32H", (ip.ROOT / "game/data/s1spr/sprpal.bin").read_bytes())
line1 = [ip.cram(w) for w in sprpal[16:32]]
pal = [ip.q3(ip.cps_rgb(0xF000 | struct.unpack_from(">H", rom, ip.TEXT_PALS + (0x0E * 16 + i) * 2)[0])) for i in range(16)]


def glyph(code):
    px = ip.decode_tile(gfx, code, 8)
    return ip.pack_tile([1 if pen == 15 else min(range(1, 16), key=lambda i: ip.dist(pal[pen], line1[i])) for row in px for pen in row])


fpath, hpath = Path("data/intro/font.bin"), Path("data/intro/intro_text.h")
font, h = bytearray(fpath.read_bytes()), hpath.read_text()
m = re.search(r'font_title\[64\] = \{([^}]*)\}', h)
tab = [int(v) for v in m.group(1).split(',')]
for arg in sys.argv[1:]:
    ch, code = arg.split("="); code = int(code, 16)
    if tab[ord(ch) - 32]: continue
    tab[ord(ch) - 32] = len(font) // 32
    for dc in (0, 1, 0x10, 0x11): font += glyph(code + dc)
fpath.write_bytes(bytes(font))
h = h.replace(m.group(0), 'font_title[64] = { ' + ', '.join(map(str, tab)) + ' }')
h = re.sub(r'#define INTRO_FONT_TILES \d+', '#define INTRO_FONT_TILES %d' % (len(font) // 32), h)
hpath.write_text(h)
print(len(font) // 32, "font tiles")
