#!/usr/bin/env python3
# fr.py KEYFILE FROM TO [extra symbols]: per-frame section lines, from out/symbol.txt and out/rom.bin
import subprocess,os,sys
H="../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1/out/menutest"
core=os.path.expanduser("~/Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
keys=open(sys.argv[1]).read().strip(); a,b=int(sys.argv[2]),int(sys.argv[3]); ex=sys.argv[4:]
sym={l.split()[-1]:l.split()[0][4:] for l in open("out/symbol.txt") if len(l.split())>=3}
pk=f"FF{sym['prof']}:16,FF{sym.get('skipped_presents',sym['prof'])}:2,"+",".join(f"FF{sym[e]}:2" for e in ex)
env=dict(os.environ,KEYS=keys,LOG="1",PEEK=pk.rstrip(','))
o=subprocess.run([H,core,"out/rom.bin",str(b+2)],env=env,capture_output=True,text=True).stdout
R=[l for l in o.split("\n") if 'peek' in l]
def ln(a,b): return ((b>>8)-(a>>8))%262
for f in range(a,b):
    p=R[f].split(); h=p[1].split(':')[1]; v=[int(h[i:i+4],16) for i in range(0,len(h),4)]
    print(f,'logic',ln(v[0],v[1]),ln(v[1],v[2]),ln(v[2],v[3]),'scroll+hud',ln(v[3],v[4]),'draw',ln(v[4],v[5]),'start',v[0]>>8,'end',v[5]>>8,'actors',ln(v[6],v[7]),'skip',int(p[2].split(':')[1],16),*[x.split(':')[1] for x in p[3:]])
