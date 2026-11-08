#!/usr/bin/env python3
"""editor_apply.py EDITS.json... [--check] [--force]: apply the pixel editor's
edits files (editor/index.html, packs from tools/editor_export.py) to the
cartridge data, then rebuild with build.sh.

Each tile edit names the data file the cartridge includes (a stage's
tiles.bin / bd_tiles.bin, a sprite family's tiles.bin), the tile number and
its 32 bytes before and after; each palette edit a CRAM word in a pal.bin or
sprpal.bin. An edit applies only when the file still holds its "before"
bytes (already holding "after" counts as applied), so a file made from
older art is refused instead of mixed in; --force writes anyway.

Every applied file is copied to data/edits/ (named after its pack), and
data/edits/ is replayed by `editor_apply.py --replay` after a packer
(tools/pack_stage*.sh, sprpal.py) regenerates the art. --check reports
without writing.
"""
import json, shutil, subprocess, sys
from pathlib import Path

G = Path(__file__).resolve().parents[1]
KEEP = G / "data" / "edits"


def check_rules(f):
    """Cartridge limits the editor enforces, checked again here."""
    errs = []
    for e in f.get("tiles", []):
        a = bytes.fromhex(e["after"])
        if len(a) != 32:
            errs.append("tile %s #%d: %d bytes, not 32" % (e["file"], e["tile"], len(a)))
            continue
        if e["asset"].startswith("sprite:") and e.get("line", 0) >= 2:
            pens = {b >> 4 for b in a} | {b & 15 for b in a}
            if max(pens) > f.get("static_pens", 15):
                errs.append("tile %s #%d: animated scenery pen on a sprite" % (e["file"], e["tile"]))
    for e in f.get("palette", []):
        w = int(e["after"], 16)
        if w & ~0x0EEE:
            errs.append("palette %s line %d pen %d: %04X is not a Mega Drive colour" % (e["file"], e["line"], e["pen"], w))
        if e["pen"] == 0 or e["line"] == 0:
            errs.append("palette %s line %d pen %d: locked entry" % (e["file"], e["line"], e["pen"]))
    return errs


def apply(f, write, force):
    files = {}
    def data(p):
        if p not in files:
            files[p] = bytearray((G / p).read_bytes())
        return files[p]
    done = already = refused = 0
    for e in f.get("tiles", []):
        d = data(e["file"])
        o = e["tile"] * 32
        cur = bytes(d[o:o + 32]).hex()
        if cur == e["after"]:
            already += 1
        elif cur == e["before"] or force:
            d[o:o + 32] = bytes.fromhex(e["after"])
            done += 1
        else:
            refused += 1
            print("  refused: %s tile %d holds other art than the file expects" % (e["file"], e["tile"]))
    for e in f.get("palette", []):
        d = data(e["file"])
        o = e["offset"]
        cur = "%04X" % ((d[o] << 8) | d[o + 1])
        if cur == e["after"].upper():
            already += 1
        elif cur == e["before"].upper() or force:
            w = int(e["after"], 16)
            d[o:o + 2] = bytes((w >> 8, w & 255))
            done += 1
        else:
            refused += 1
            print("  refused: %s line %d pen %d is %s, the file expects %s" % (e["file"], e["line"], e["pen"], cur, e["before"]))
    if write and not refused and done:
        for p, d in files.items():
            (G / p).write_bytes(bytes(d))
            for s in (G / "src").glob("*.s"):      # make does not track .incbin'ed data
                if p in s.read_text():
                    s.touch()
    return done, already, refused, sorted(files)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    write = "--check" not in sys.argv
    force = "--force" in sys.argv
    if "--replay" in sys.argv:
        args = sorted(str(p) for p in KEEP.glob("*.json"))
    bad = 0
    for path in args:
        f = json.loads(Path(path).read_text())
        if f.get("format") != "willow-pixel-edits":
            print("%s: not a Willow pixel edits file" % path); bad += 1; continue
        errs = check_rules(f)
        for e in errs:
            print("  " + e)
        if errs:
            print("%s: rejected (%d rule errors)" % (path, len(errs))); bad += 1; continue
        done, already, refused, touched = apply(f, write, force)
        print("%s (%s): %d applied, %d already in, %d refused; files: %s" % (
            path, f.get("level"), done, already, refused, ", ".join(touched) or "none"))
        if refused:
            print("  nothing written for this file (re-export from the current editor packs, or --force)")
            bad += 1
        elif write and "--replay" not in sys.argv:
            KEEP.mkdir(parents=True, exist_ok=True)
            dst = KEEP / Path(path).name
            if Path(path).resolve() != dst.resolve():
                shutil.copyfile(path, dst)
    if write and args and not bad:
        subprocess.run([sys.executable, str(G / "tools" / "editor_export.py")], check=True)   # editor packs follow the data
        print("Now rebuild: sh build.sh")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
