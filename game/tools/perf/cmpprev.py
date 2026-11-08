#!/usr/bin/env python3
# cmpprev.py STAGEDIR NEWDIR: pixels of the packed map: kept, changed, added (preview.ppm: black = empty)
import sys
def rd(f):
    p=open(f,'rb').read().split(b"\n",3); w,h=map(int,p[1].split()); return w,h,p[3]
wa,ha,a=rd(sys.argv[1]+'/preview.ppm'); wb,hb,b=rd(sys.argv[2]+'/preview.ppm')
same=ch=add=0
for i in range(0,min(len(a),len(b)),3):
    x=a[i:i+3]; y=b[i:i+3]
    if x==b'\0\0\0':
        if y!=b'\0\0\0': add+=1
    elif x==y: same+=1
    else: ch+=1
print(wa,ha,wb,hb,'kept',same,'changed',ch,'added',add)
