#!/usr/bin/env python3
"""crop.py OUT.png X Y W H SCALE ppm...: the same crop of several PPMs stacked, scaled."""
import subprocess, sys, os
out=sys.argv[1]; X,Y,W,H,Z=map(int,sys.argv[2:7]); fs=sys.argv[7:]
o=bytearray()
for f in fs:
    p=open(f,'rb').read().split(b"\n",3); w,h=map(int,p[1].split()); px=p[3]
    for y in range(Y,Y+H):
        l=bytearray()
        for x in range(X,X+W): l+=px[(y*w+x)*3:(y*w+x)*3+3]*Z
        o+=l*Z
    o+=b'\xff\x00\xff'*(W*Z)*3
open(out+'.ppm','wb').write(b"P6\n%d %d\n255\n"%(W*Z,(H*Z+3)*len(fs))+o)
subprocess.run(["sips","-s","format","png",out+".ppm","--out",out],check=True,capture_output=True); os.remove(out+".ppm")
