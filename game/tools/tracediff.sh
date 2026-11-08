#!/bin/sh
# tracediff.sh NAME FROM TO: arcade and native Willow state side by side.
cd "$(dirname "$0")/.." || exit 1
H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
CORE="$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib"
A=$(grep " player$" out/symbol.txt | awk '{print substr($1,5)}')
T=100
K=$(tr ';' '\n' < tests/traces/$1.keys | awk -F: -v t=$T '{split($1,a,"-"); printf "%d-%d:%s;", a[1]+t, a[2]+t, $2}')
KEYS="$K" LOG=1 PEEK="FF$A:64" "$H" "$CORE" out/rom.bin $((T + $3 + 2)) 2>&1 | grep peek | awk -v t=$T '{print NR-t-1, $2}' > /tmp/td.$$.txt
python3 - tests/traces/$1.ref /tmp/td.$$.txt $2 $3 <<'P'
import sys, json
nodes = [n['source'] for n in json.load(open('../build/native-world/player-hold-data-v1/manifest.json'))['nodes']]
a, b = int(sys.argv[3]), int(sys.argv[4])
nat = {}
for l in open(sys.argv[2]):
    f, r = l.split()[:2]; nat[int(f)] = bytes.fromhex(r.split(':')[1])
for l in open(sys.argv[1]):
    f, r, c = l.split(); f = int(f)
    if not a <= f <= b: continue
    r = bytes.fromhex(r.split(':')[1]); c = bytes.fromhex(c.split(':')[1])
    p = int.from_bytes(r[20:24], 'big')
    print('A %4d st %s x %4d y %4d.%02x pose %3s cam %4d %4d' % (f, r[2:6].hex(), int.from_bytes(r[6:8], 'big'), int.from_bytes(r[10:12], 'big'), r[12], nodes.index(p) if p in nodes else '?', int.from_bytes(c[0:2], 'big'), int.from_bytes(c[4:6], 'big')))
    n = nat.get(f)
    if n:
        g = lambda o: int.from_bytes(n[o:o+2], 'big')
        print('N %4d act %d ph %d   x %4d y %4d.%02x pose %3d cam %4d %4d vy %d' % (f, g(12), g(38), int.from_bytes(n[0:4], 'big') >> 8, int.from_bytes(n[4:8], 'big') >> 8, n[7], g(24), g(32), g(34), g(8)))
P
rm -f /tmp/td.$$.txt
