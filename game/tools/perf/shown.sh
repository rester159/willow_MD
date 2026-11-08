#!/bin/sh
# shown.sh FLAGS KEYSTRING FRAMES: game updates and shown frames per 100 frames (worst / median windows)
cd "/Users/rester159/genesis ports/_capcom/willow/game"
sh tools/perf/b.sh "$1" > /dev/null || exit 1
G=$(grep " game_frame$" out/symbol.txt | awk '{print substr($1,5)}'); K=$(grep " skipped_presents$" out/symbol.txt | awk '{print substr($1,5)}'); H=../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest
KEYS="$2" LOG=1 PEEK="FF$G:2,FF$K:2" $H "$HOME/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib" out/rom.bin $3 2>&1 | grep peek | python3 -c "
import sys,statistics
g=[];k=[]
for l in sys.stdin:
    e=l.split()[1:]; g.append(int(e[0].split(':')[1],16)); k.append(int(e[1].split(':')[1],16))
u=[(g[s+100]-g[s])&0xFFFF for s in range(300,len(g)-100,100)]
d=[u[i]-((k[s+100]-k[s])&0xFFFF) for i,s in enumerate(range(300,len(g)-100,100))]
p=[(a,b) for a,b in zip(u,d) if a>=90]; u=[a for a,b in p]; d=[b for a,b in p]
print(' '.join('%d/%d'%x for x in p))
print('in play %d windows: updates/100 worst %d median %d | shown/100 worst %d median %d' % (len(p),min(u),statistics.median(u),min(d),statistics.median(d)))"
