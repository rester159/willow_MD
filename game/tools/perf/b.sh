#!/bin/sh
# b.sh FLAGS: build; fail loudly when the ROM was not relinked
cd "/Users/rester159/genesis ports/_capcom/willow/game"
touch -t 200001010000 out/rom.bin 2>/dev/null
CFLAGS_EXTRA="$1" ./build.sh > out/perf/b.log 2>&1
if grep -qi "error" out/perf/b.log || [ "$(find out/rom.bin -newer src/main.c 2>/dev/null)" = "" ]; then echo "BUILD FAILED"; grep -i -A3 "error" out/perf/b.log | head -20; exit 1; fi
echo "built ($1) $(stat -f %z out/rom.bin) bytes"
