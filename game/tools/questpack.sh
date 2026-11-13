#!/bin/sh
# questpack.sh: aPLib-pack Sorsha's Quest pictures (tools/questmap.py output):
# maps as one stream padded to even (src/packed.c map_open), tiles in the
# chunk format of tools/pack_compress.sh (src/packed.c load_packed).
cd "$(dirname "$0")/.." || exit 1
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; APJ=../build/sgdk-native/bin/apj.jar
python3 - "$JAVA" "$APJ" <<'PY'
import struct, subprocess, sys, tempfile
from pathlib import Path
java, apj = sys.argv[1:3]
t = Path(tempfile.mkdtemp())
def pp(d):
    Path(t, "c.bin").write_bytes(d)
    subprocess.run([java, "-jar", apj, "pp", str(t / "c.bin"), str(t / "c.apj")], capture_output=True, check=True)
    p = Path(t, "c.apj").read_bytes()
    return p + b"\0" if len(p) & 1 else p
for f in ["data/quest/qmap_map.bin", "data/quest/p1_nockmaar_map.bin", "data/quest/p6_return_map.bin", "data/quest/p7_ending_map.bin"]:
    d = Path(f).read_bytes(); o = pp(d); Path(f[:-4] + ".apj").write_bytes(o); print(f, len(d), "->", len(o))
for f in ["data/quest/qmap_xtiles.bin", "data/quest/p1_nockmaar_tiles.bin", "data/quest/p6_return_tiles.bin", "data/quest/p7_ending_tiles.bin"]:
    d = Path(f).read_bytes()
    out = struct.pack(">H", (len(d) + 8191) // 8192)
    for i in range(0, len(d), 8192):
        c = d[i:i + 8192]; p = pp(c); out += struct.pack(">HH", len(c) // 32, len(p)) + p
    Path(f[:-4] + ".apj").write_bytes(out); print(f, len(d), "->", len(out))
PY
