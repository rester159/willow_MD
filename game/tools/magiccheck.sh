#!/bin/sh
# magiccheck.sh [NAME...]: run each scripted input that has a magic
# reference (tests/traces/NAME.mag) on out/rom.bin headlessly and compare
# the charge state and the shots with the arcade (tools/magiccmp.py).
cd "$(dirname "$0")/.." || exit 1
H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
CORE="$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
M=$(grep " magic$" out/symbol.txt | awk '{print substr($1,5)}')
S=$(grep " shots$" out/symbol.txt | awk '{print substr($1,5)}')
T=100
[ $# -eq 0 ] && set -- $(ls tests/traces/*.mag | sed 's#.*/##; s#\.mag##')
for n in "$@"; do
  K=$(tr ';' '\n' < tests/traces/$n.keys | awk -F: -v t=$T '{split($1,a,"-"); printf "%d-%d:%s;", a[1]+t, a[2]+t, $2}')
  F=$(tail -1 tests/traces/$n.mag | awk '{print $1}')
  KEYS="$K" LOG=1 PEEK="FF$M:8,FF$S:96" "$H" "$CORE" out/rom.bin $((T + F + 2)) 2>&1 | grep peek |
    awk -v t=$T '{n=NR; if (n>=t-5) print n-t, $2, $3}' > /tmp/magiccheck.$$.txt
  echo "== $n (magic)"
  python3 tools/magiccmp.py tests/traces/$n.mag /tmp/magiccheck.$$.txt $OPTS | tail -n +2
  rm -f /tmp/magiccheck.$$.txt
done
