import sys
stage,cp,hold=int(sys.argv[1]),int(sys.argv[2]),sys.argv[3]
# opening: Down (HOME), A; Home: Up Up Down Down C (debug); stage, point, invincible, start
seq=['D','A','U','U','D','D','C']+['R']*(stage-1)+['D']+['R']*cp+['D','R','D','A']
t=60; out=[]
for k in seq: out.append(f"{t}-{t+2}:{k}"); t+=12
print(";".join(out)+f";{t+300}-{t+2700}:{hold}")
