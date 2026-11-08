# the interior-top script (walk + attack every 8 + jumps) after the debug page's start at point 10
import sys
sys.argv=[sys.argv[0],'5','10','R']
seq=['D','A','U','U','D','D','C']+['R']*4+['D']+['R']*10+['D','R','D','A']
t=60; out=[]
for k in seq: out.append(f"{t}-{t+2}:{k}"); t+=12
t+=300
d=t-100
s=open('out/perf/s5top.keys').read().strip().rstrip(';')
for e in s.split(';'):
    r,b=e.split(':'); a,c=r.split('-'); out.append(f"{int(a)+d}-{int(c)+d}:{b}")
print(";".join(out))
