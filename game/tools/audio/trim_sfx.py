#!/usr/bin/env python3
"""trim_sfx.py: cut each FM effect render (data/audio/render/sfx*_opm.wav)
to its arcade effect length (the hold in src/sound.c's sfx_table, 60 Hz
frames) plus half a second, with a 50 ms fade: the renders keep the FM
release tails (up to 3 s) that the ROM cannot afford. Originals are kept
as .full and re-read on later runs. Run after tools/audio/run*.sh."""
import array, os, re, wave
from pathlib import Path

game = Path(__file__).resolve().parents[2]
res = (game / "res/audio.res").read_text()
tab = (game / "src/sound.c").read_text()
files = {m.group(1): game / m.group(2) for m in re.finditer(r'WAV (\w+) "\.\./(data/audio/render/sfx\w+\.wav)" XGM2', res)}
holds = {m.group(3): int(m.group(2)) for m in re.finditer(r'\bE\((\w+),\s*\w+,\s*\d+,\s*(\d+),\s*(\w+)\)', tab)}
for name, f in files.items():
    if name not in holds: continue
    src = Path(str(f) + ".full") if Path(str(f) + ".full").exists() else f
    w = wave.open(str(src)); n, r, c, sw = w.getnframes(), w.getframerate(), w.getnchannels(), w.getsampwidth(); d = w.readframes(n); w.close()
    keep = int(((holds[name] + 30) / 60.0) * r)
    if keep >= n: continue
    if src == f: os.rename(f, str(f) + ".full")
    a = array.array("h", d)[:keep * c]
    fade = int(0.05 * r)
    for i in range(fade):
        for ch in range(c): a[(keep - fade + i) * c + ch] = int(a[(keep - fade + i) * c + ch] * (fade - i) / fade)
    w = wave.open(str(f), "wb"); w.setnchannels(c); w.setsampwidth(sw); w.setframerate(r); w.writeframes(a.tobytes()); w.close()
    print(f"{name}: {n / r:.2f} s -> {keep / r:.2f} s")
