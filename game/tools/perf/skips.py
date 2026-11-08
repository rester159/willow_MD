#!/usr/bin/env python3
import subprocess,os,sys
H="../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest"
core=os.path.expanduser("~/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
keys=open(sys.argv[1]).read().strip(); N=int(sys.argv[2])
sym={l.split()[-1]:l.split()[0][4:] for l in open("out/symbol.txt") if len(l.split())>=3}
ex=['skipped_presents','posecache_loads','posecache_misses','stage_queued','stage_vq_words','draw_est','ct_room','ct_rebuild','ct_max']
pk=f"FF{sym['prof']}:16,"+",".join(f"FF{sym[e]}:2" for e in ex if e in sym)
print([e for e in ex if e in sym])
env=dict(os.environ,KEYS=keys,LOG="1",PEEK=pk)
o=subprocess.run([H,core,"out/rom.bin",str(N)],env=env,capture_output=True,text=True).stdout
R=[l.split() for l in o.split("\n") if 'peek' in l]
ps=None
def ln(a,b): return ((b>>8)-(a>>8))%262
for f,p in enumerate(R):
    h=p[1].split(':')[1]; v=[int(h[i:i+4],16) for i in range(0,len(h),4)]
    e=[int(x.split(':')[1],16) for x in p[2:]]
    s=e[0]
    if ps is not None and s!=ps: print(f,'skip',s,'start',v[0]>>8,'end',v[5]>>8,'draw',ln(v[4],v[5]),e[1:])
    ps=s
