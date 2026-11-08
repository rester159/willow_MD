#!/bin/sh
# pack_compress.sh: aPLib-pack the pictures loaded only at scene changes
# (world maps, story pages, the shop, the attract loop's pictures) into
# FILE.apj next to each source, in chunks of 256 tiles (an 8 KB heap buffer
# each): u16 chunks, then per chunk u16 tiles, u16 packed bytes (even), the
# packed data. src/packed.c unpacks them chunk by chunk into VRAM.
cd "$(dirname "$0")/.." || exit 1
JAVA=/opt/homebrew/opt/openjdk@21/bin/java; APJ=../build/sgdk-native/bin/apj.jar
T=$(mktemp -d)
for f in data/intro/map_tiles.bin data/intro/map2_tiles.bin data/intro/map3_tiles.bin data/intro/map4_tiles.bin data/intro/map5_tiles.bin data/intro/map6_tiles.bin \
         data/intro/story_tiles.bin data/intro/story2_tiles.bin data/intro/story3_tiles.bin data/intro/story4_tiles.bin data/intro/story5_tiles.bin data/intro/story6_tiles.bin \
         data/ending/storye0_tiles.bin data/ending/storye1_tiles.bin data/ending/storye2_tiles.bin data/ending/storye3_tiles.bin data/ending/storye4_tiles.bin data/ending/storye5_tiles.bin \
         data/ending/end_sky_tiles.bin data/ending/end_castle_tiles.bin data/ending/end_card_tiles.bin \
         data/shop/tiles.bin data/attract/att_*_tiles.bin data/contfig/tiles0.bin data/contfig/tiles1.bin; do
  python3 - "$f" "$T" "$JAVA" "$APJ" <<'PY'
import struct, subprocess, sys
from pathlib import Path
f, t, java, apj = sys.argv[1:5]
d = Path(f).read_bytes()
chunks = [d[i:i + 8192] for i in range(0, len(d), 8192)]
out = struct.pack(">H", len(chunks))
for k, c in enumerate(chunks):
    Path(t, "c.bin").write_bytes(c)
    subprocess.run([java, "-jar", apj, "pp", str(Path(t, "c.bin")), str(Path(t, "c.apj"))], capture_output=True, check=True)
    p = Path(t, "c.apj").read_bytes()
    if len(p) & 1: p += b"\0"
    out += struct.pack(">HH", len(c) // 32, len(p)) + p
Path(f[:-4] + ".apj").write_bytes(out)
print(f"{f}: {len(d)} -> {len(out)} ({len(chunks)} chunks)")
PY
done
rm -rf "$T"
