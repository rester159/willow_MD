#!/bin/bash
cd "/Users/rester159/genesis ports/_capcom/willow/game"
H=coop/tools/menutest2
K="60-62:D;100-102:A;140-142:U;160-162:U;180-182:D;200-202:D;220-222:C;240-242:D;260-262:D;280-282:R;300-302:D;320-322:A;2300-2303:S;3600-3603:S"
K2=""; [ "$2" = 1 ] && K2="2000-2003:S"
rm -rf $1; mkdir -p $1
KEYS="$K" KEYS2="$K2" DUMP=2280,2340,2940,3560,3640,3900 DIR="$1" "$H" "$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" willow-native-m2-20261106p.bin 3910 2>&1 | tail -1
