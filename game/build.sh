#!/bin/sh
# Build out/rom.bin. data.s pulls in data/ with .incbin, which make does not
# track, so it is always reassembled.
cd "$(dirname "$0")" || exit 1
export PATH="/opt/homebrew/opt/openjdk@21/bin:$PATH"   # rescomp runs xgm2tool with java
# Two-player co-op (Home) is part of the game: every build has it, unless
# CFLAGS_EXTRA says -DNOCOOP (measurement builds without the co-op code).
case " $CFLAGS_EXTRA " in *" -DNOCOOP "*|*" -DCOOP "*) ;; *) CFLAGS_EXTRA="-DCOOP $CFLAGS_EXTRA";; esac
touch src/data.s src/s1spr.s src/s2spr.s src/main.c
# make does not track the flags: rebuild every object when CFLAGS_EXTRA changes.
if [ "$(cat out/.flags 2>/dev/null)" != "$CFLAGS_EXTRA" ]; then rm -f out/src/*.o; mkdir -p out; echo "$CFLAGS_EXTRA" > out/.flags; fi
make EXTRA_FLAGS="$CFLAGS_EXTRA" -f tools/makefile_willow.gen SGDK=../build/sgdk-native JAVA=/opt/homebrew/opt/openjdk@21/bin/java release 2>&1 | grep -i "error\|warning"
# The header checksum over the whole image (sizebnd's did not match), and the
# mapper probes' assumptions (src/cart.c): bank 1's word at 0x100 differs
# from the header's "SE", bank 8's first long from bank 6's.
python3 - <<'PYEOF'
import struct
p = "out/rom.bin"
b = bytearray(open(p, "rb").read())
s = 0
for i in range(0x200, len(b), 2):
    s = (s + (b[i] << 8 | b[i + 1])) & 0xFFFF
b[0x18E:0x190] = struct.pack(">H", s)
open(p, "wb").write(b)
assert b[0x80100:0x80102] != b[0x100:0x102], "cart probe: bank 1 at 0x100 reads like the header"
assert b[0x300000:0x300004] != b[0x400000:0x400004], "cart probe: bank 8 starts like bank 6"
PYEOF
ls -la out/rom.bin | awk '{print "out/rom.bin", $5, "bytes"}'
