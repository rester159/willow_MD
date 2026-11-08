#!/bin/sh
# headless.sh ROM FRAMES DUMPFRAMES OUTDIR [KEYS]: run the ROM on the stock
# Genesis Plus GX libretro core with no window (B1's menutest harness) and
# dump the listed frames as OUTDIR/fNNNNNN.ppm.
H="$(cd "$(dirname "$0")/.." && pwd)/../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest"
CORE="$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
mkdir -p "$4"
KEYS="$5" DUMP="$3" DIR="$4" "$H" "$CORE" "$1" "$2"
