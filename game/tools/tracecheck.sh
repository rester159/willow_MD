#!/bin/sh
# tracecheck.sh [NAME...]: run each scripted input (tests/traces/NAME.keys)
# on out/rom.bin headlessly and compare Willow with the arcade reference
# (tests/traces/NAME.ref: the original program at full speed, lockstep
# ref + SCRIPT=keys, see M1_NOTES.md). Default: every trace.
cd "$(dirname "$0")/.." || exit 1
H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
CORE="$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
A=$(grep " player$" out/symbol.txt | awk '{print substr($1,5)}')
T=100
[ $# -eq 0 ] && set -- $(ls tests/traces/*.keys | sed 's#.*/##; s#\.keys##')
for n in "$@"; do
  K=$(tr ';' '\n' < tests/traces/$n.keys | awk -F: -v t=$T '{split($1,a,"-"); printf "%d-%d:%s;", a[1]+t, a[2]+t, $2}')
  F=$(tail -1 tests/traces/$n.ref | awk '{print $1}')
  KEYS="$K" LOG=1 PEEK="FF$A:40" "$H" "$CORE" out/rom.bin $((T + F + 2)) 2>&1 | grep peek |
    awk -v t=$T '{n=NR; if (n>=t-5) print n-t, $2}' > /tmp/tracecheck.$$.txt
  echo "== $n"
  python3 tools/tracecmp.py tests/traces/$n.ref /tmp/tracecheck.$$.txt --shift=1 | tail -n +2
  rm -f /tmp/tracecheck.$$.txt
done
