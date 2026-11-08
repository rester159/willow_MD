#!/usr/bin/env python3
"""editor_export.py [STAGE...]: the level pixel editors' data packs
(editor/packs/stageN.js, editor/packs/heroes.js) from the cartridge's own
data files, the ones src/*.s includes.

Each pack is `window.WILLOW_PACK = {...}`: the stage's CRAM lines (sprite
lines 0-1 from sprpal.bin, scenery lines 2-3 from pal.bin), per area the
foreground tiles and a map of tile references, the far layer's tiles and
map, and every sprite family in the cartridge with its poses and tiles.
Binary data is base64; maps are zlib-compressed.

Foreground maps name VRAM slots (stagepack.py streaming); the tile in a
cell is the last tile loaded into its slot at a right-edge column <= the
cell's column (runs sharing a slot are never on screen together). Map
cells are exported as u16: tile | far/line-3 bit 12 | H-flip 13 | V-flip 14,
0xFFFF empty.

The editor's output is applied with tools/editor_apply.py.
"""
import base64, json, re, struct, sys, zlib
from pathlib import Path

G = Path(__file__).resolve().parents[1]
OUT = G / "editor" / "packs"

STAGES = {
    1: dict(name="Stage 1 - The Forest", areas=[("", "data/stage1")], spr="s1spr", sprs="src/s1spr.s",
            far=(265, 28, 703), cycle="data/stage1/cycle.bin", sky=34),
    2: dict(name="Stage 2 - The Chase", areas=[("", "data/stage2")], spr="s2spr", sprs="src/s2spr.s",
            far=(512, 45, 703), sky=None, lock3="its colours change during the chase (far_pal.bin)"),
    3: dict(name="Stage 3 - The Lake", areas=[("", "data/stage3")], spr="s3spr", sprs="src/s3spr.s",
            far=(408, 46, 703), cycle="data/stage3/cycle.bin", sky=33),
    4: dict(name="Stage 4 - The Snow Mountain", areas=[("", "data/stage4")], spr="s4spr", sprs="src/s4spr.s",
            far=(32, 111, 297), sky=40),
    5: dict(name="Stage 5 - Tir Asleen", areas=[("", "data/stage5")], spr="s5spr", sprs="src/s5spr.s",
            far=(404, 55, 509), sky=44),
    6: dict(name="Stage 6 - Nockmaar Castle", spr="s6spr", sprs="src/s6spr.s",
            areas=[("Area %d" % i, "data/stage6/a%d" % i) for i in range(7)],
            far6=[(168, 28, 273), (173, 28, 298), (168, 28, 291), (173, 28, 232), (45, 28, 169), (83, 158, 585), None],
            sky6=[52, 52, 54, 47, 34, 44, 0]),
}

ITEMS = {"chest", "gold", "hidden", "orb"}
EFFECTS = {"arrow", "axe", "spear", "fireball", "squiggle", "burst", "splash", "sparkle", "spark", "shot", "spell",
           "spell1", "bossfire", "bolt3", "bomb", "flame", "fire", "smoke", "debris", "glow", "clash", "crash",
           "flake", "snowball", "lance", "kslash", "tspear", "doorfx", "emerge", "rock", "cursor", "mortar", "stone"}
SCENERY = {"log", "platform", "windmill", "pillar", "door", "doorway", "shutter", "chandelier", "chand", "cart",
           "altar", "tent", "boat", "raft", "crusher", "wagon", "figure", "flail", "ending"}
BOSSES = {"bosshead", "bosspaw", "boss3", "serpent", "dragon", "bav", "kael", "head", "troll"}


def b64(b):
    return base64.b64encode(bytes(b)).decode()


def z64(b):
    return b64(zlib.compress(bytes(b), 9))


def words(p):
    d = Path(p).read_bytes()
    return list(struct.unpack(">%dH" % (len(d) // 2), d))


def category(name):
    if name in ITEMS: return "items"
    if name in EFFECTS: return "effects"
    if name in SCENERY: return "objects"
    if name in BOSSES: return "bosses"
    return "characters"


def fg_area(d):
    """Foreground: tiles, cell map (tile refs), per-tile line."""
    tiles = (d / "tiles.bin").read_bytes()
    nt = len(tiles) // 32
    lw = words(d / "loads.bin")
    C = lw[0]
    offs = lw[2:2 + 2 * C + 1]
    ent0 = 2 + 2 * C + 1
    mw = words(d / "map.bin")
    rows = len(mw) // C
    slot_tile = {}
    cells = [0xFFFF] * (C * rows)
    line_of = [None] * nt
    bad = 0
    for c in range(C):
        for e in range(offs[c], offs[c + 1]):
            t, s = lw[ent0 + 2 * e], lw[ent0 + 2 * e + 1]
            slot_tile[s] = t
        for r in range(rows):
            w = mw[c * rows + r]
            if not w:
                continue
            slot = (w & 0x7FF) - 16
            t = slot_tile.get(slot)
            if t is None:
                bad += 1
                continue
            line = (w >> 13) & 3
            if line_of[t] is None:
                line_of[t] = line
            elif line_of[t] != line:
                bad += 1
            cells[c * rows + r] = t | ((line == 3) << 12) | (((w >> 11) & 1) << 13) | (((w >> 12) & 1) << 14)
    if bad:
        print("  %s: %d unresolved/mixed-line cells" % (d, bad), file=sys.stderr)
    return dict(tiles=b64(tiles), cols=C, rows=rows, map=z64(struct.pack(">%dH" % len(cells), *cells)),
                lines=[l if l is not None else 2 for l in line_of], used=sum(1 for l in line_of if l is not None))


def far_area(d, fc, fr, base):
    tiles = (d / "bd_tiles.bin").read_bytes()
    nt = len(tiles) // 32
    mw = words(d / "bd_map.bin")
    if len(mw) < fc * fr:
        return None
    cells = []
    line_of = [None] * nt
    for w in mw[:fc * fr]:
        t = (w & 0x7FF) - base
        if not 0 <= t < nt:
            cells.append(0xFFFF)
            continue
        line = (w >> 13) & 3
        line_of[t] = line if line_of[t] is None else line_of[t]
        cells.append(t | ((line == 3) << 12) | (((w >> 11) & 1) << 13) | (((w >> 12) & 1) << 14))
    return dict(tiles=b64(tiles), cols=fc, rows=fr, map=z64(struct.pack(">%dH" % len(cells), *cells)),
                lines=[l if l is not None else 2 for l in line_of])


def family(path, name, cat=None):
    d = G / path
    lay = (d / "layout.h").read_text()
    m = re.search(r"#define \w+_(?:COUNT|POSES) (\d+)", lay)
    n = int(m.group(1))
    poses = (d / "poses.bin").read_bytes()
    tiles = (d / "tiles.bin").read_bytes()
    return dict(name=name, file=str(path) + "/tiles.bin", cat=cat or category(name), count=n,
                poses=b64(poses), tiles=b64(tiles))


def families_of(sfile):
    seen = []
    for m in re.finditer(r'"(data/s\dspr/(\w+))/tiles.bin"', (G / sfile).read_text()):
        if m.group(1) not in [s[0] for s in seen]:
            seen.append((m.group(1), m.group(2)))
    return [family(p, n) for p, n in seen]


def stage_pack(n):
    st = STAGES[n]
    spal = words(G / "data" / st["spr"] / "sprpal.bin")
    areas = []
    for i, (aname, ad) in enumerate(st["areas"]):
        d = G / ad
        pal = words(d / "pal.bin")
        cram = spal[:32] + pal[:32]
        anim = []
        if st.get("cycle"):
            cyc = words(G / st["cycle"])
            for k in range(6):          # first state of the cycle on pens 10-15
                cram[32 + 10 + k] = cyc[k]
                cram[48 + 10 + k] = cyc[6 + k]
            anim = list(range(42, 48)) + list(range(58, 64))
        far = st["far6"][i] if "far6" in st else st["far"]
        sky = st["sky6"][i] if "sky6" in st else st["sky"]
        if sky is None:
            sky = struct.unpack(">H", (d / "bd_sky.bin").read_bytes())[0]
        a = dict(name=aname or st["name"], dir=ad, cram=cram, anim=anim, sky=sky, fg=fg_area(d),
                 far=far_area(d, *far) if far else None)
        print("  %s: fg %d tiles (%d used), %dx%d cells; far %s" % (
            ad, len(a["fg"]["lines"]), a["fg"]["used"], a["fg"]["cols"], a["fg"]["rows"],
            "%dx%d" % (a["far"]["cols"], a["far"]["rows"]) if a["far"] else "none"), file=sys.stderr)
        areas.append(a)
    locked = {0: "Willow's colours: shared by every level and used for her hurt flash"}
    if st.get("lock3"):
        locked[3] = st["lock3"]
    return dict(kind="stage", stage=n, name=st["name"], sprpal=st["spr"] + "/sprpal.bin",
                staticPens=9 if st.get("cycle") else 15, locked=locked, areas=areas,
                families=families_of(st["sprs"]))


def heroes_pack():
    spal = words(G / "data/s1spr/sprpal.bin")
    pal = words(G / "data/stage1/pal.bin")
    mad = family("data/mad", "madmartigan", "characters")
    mad["pal0"] = words(G / "data/mad/pal.bin")     # his own line 0 (src/game.c s5/s6_sprpal_live)
    fams = [family("data/willow", "willow", "characters"), mad,
            family("data/shot", "shot", "effects"), family("data/spark", "spark", "effects")]
    return dict(kind="heroes", stage=0, name="Heroes (every level)", sprpal="s1spr/sprpal.bin", staticPens=9,
                locked={0: "Willow's colours: shared by every level", 1: "this line belongs to each level; edit it there",
                        2: "scenery colours of stage 1", 3: "scenery colours of stage 1"},
                areas=[dict(name="Heroes", dir="", cram=spal[:32] + pal[:32], anim=[], sky=0, fg=None, far=None)],
                families=fams)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    which = [int(a) for a in sys.argv[1:]] or [0] + sorted(STAGES)
    for n in which:
        pack = heroes_pack() if n == 0 else stage_pack(n)
        name = "heroes" if n == 0 else "stage%d" % n
        js = "window.WILLOW_PACK = " + json.dumps(pack, separators=(",", ":")) + ";\n"
        (OUT / (name + ".js")).write_text(js)
        print("%s.js %d bytes, %d sprite families" % (name, len(js), len(pack["families"])), file=sys.stderr)


if __name__ == "__main__":
    main()
