# peaks.py LABEL:DIR ...: per arcade frame dump, distinct 8x8 tiles on screen (flip-canonical) per layer, and distinct colours; peak per label
import sys, glob, os
sys.path.insert(0, '../build/native-world/cartridge-output-binding-v1/candidate-renderer-v1/perf/arcade/b1')   # cpsframe.py
import cpsframe as cf
W,H,L,T=cf.W,cf.H,cf.LEFT,cf.TOP
def quads(t, size, attr):
    fh, fv = attr & 32, attr & 64
    out=[]
    for qy in range(0,size,8):
        for qx in range(0,size,8):
            q=tuple(tuple(t[size-1-(qy+y) if fv else qy+y][size-1-(qx+x) if fh else qx+x] for x in range(8)) for y in range(8))
            if all(p==15 for r in q for p in r): continue
            forms=[q, tuple(r[::-1] for r in q), q[::-1], tuple(r[::-1] for r in q[::-1])]
            out.append(min(forms))
    return out
def layer_tiles(d, n):
    base=(d.reg(0x800100+2*n)<<8)&0xFFFFFF; size={2:16,3:32}[n]
    sx=d.reg({2:0x800110,3:0x800114}[n]); sy=d.reg({2:0x800112,3:0x800116}[n])
    lay=d.reg(0x800170); vid=d.reg(0x800122)
    if n==2 and not (lay&0x10 and vid&4): return set()
    if n==3 and not (lay&0x08 and vid&8): return set()
    seen=set(); res=set()
    for y in range(0,H+size,size):
        yy=(y+T+sy)%(64*size); row=yy//size
        for x in range(0,W+size,size):
            xx=(x+L+sx)%(64*size); col=xx//size
            idx=(row&15)+(col<<4)+((row&48)<<6) if n==2 else (row&7)+(col<<3)+((row&56)<<6)
            a=base+idx*4; code,attr=d.word(a),d.word(a+2)
            if (code,attr) in seen: continue
            seen.add((code,attr))
            if n==2: t=cf.tile(0x4000+(code&0x1FFF),16)
            else:
                code&=0x3FFF
                if not 0xA00<=code<=0xDFF: continue
                t=cf.tile(code,32)
            for q in quads(t,size,attr): res.add((q,attr&31))
    return res
def sprite_tiles(d):
    base=(d.reg(0x800100)<<8)&0xFFFFFF; res=set(); n=0
    for at in range(0,0x800,8):
        x,y,code,a=(d.word(base+at+k) for k in (0,2,4,6))
        if a&0xFF00==0xFF00: break
        nx,ny=((a>>8)&15)+1,(a>>12)+1
        for cy in range(ny):
            for cx in range(nx):
                px,py=((x+16*cx)&511)-96,((y+16*cy)&511)-16
                if not (-16<px<320 and -16<py<224): continue
                c=(code&0xFFF0)+((code+cx)&15)+16*cy
                n+=1
                for q in quads(cf.tile(c,16),16,a): res.add((q,a&31))
    return res,n
def colours(p):
    b=open(p,'rb').read(); parts=b.split(b'\n',3); return len({parts[3][i:i+3] for i in range(0,len(parts[3]),3)})
for arg in sys.argv[1:]:
    lab,dr=arg.split(':',1)
    peak={'s2':(0,''),'s3':(0,''),'spr':(0,''),'pieces':(0,''),'all':(0,''),'col':(0,'')}
    for f in sorted(glob.glob(dr+'/*.cps')):
        d=cf.Dump(f); k=os.path.basename(f)[1:7]
        a=layer_tiles(d,2); b=layer_tiles(d,3); c,n=sprite_tiles(d)
        ppm=f[:-4]+'.ppm'; col=colours(ppm) if os.path.exists(ppm) else 0
        for name,v in (('s2',len(a)),('s3',len(b)),('spr',len(c)),('pieces',n),('all',len(a)+len(b)+len(c)),('col',col)):
            if v>peak[name][0]: peak[name]=(v,k)
    print(lab, ' '.join(f'{n}={v}@{k}' for n,(v,k) in peak.items()))
