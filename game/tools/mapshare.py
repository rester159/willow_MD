# mapshare.py: maps 2-6 re-expressed over map 1's tiles and palette, plus
# per-map extra tiles (refit to map 1's palette lines, deduped with flips).
import struct, sys
D='data/intro/'; O='data/intro/share/'
def rd(n):
    t=open(D+n+'_tiles.bin','rb').read()
    m=list(struct.unpack('>1120H',open(D+n+'_map.bin','rb').read()))
    p=struct.unpack('>64H',open(D+n+'_pal.bin','rb').read())
    a=open(D+n+'_alt.bin','rb').read(); a=list(struct.unpack('>%dH'%(len(a)//2),a))
    return t,m,p,a
def rgb(w): return (((w>>1)&7)*36,((w>>5)&7)*36,((w>>9)&7)*36)
def pens(t,i):
    o=[]
    for y in range(8):
        for x in range(8):
            b=t[i*32+y*4+x//2]; o.append((b>>4) if x%2==0 else b&15)
    return o
def cellrgb(t,p,w):
    i=w&0x7ff; pl=(w>>13)&3; hf=w&0x800; vf=w&0x1000
    pp=pens(t,i); o=[]
    for y in range(8):
        for x in range(8):
            sx=7-x if hf else x; sy=7-y if vf else y
            o.append(rgb(p[pl*16+pp[sy*8+sx]]))
    return o
def flips(pp):
    r=[]
    for f in range(4):
        q=[pp[(7-y if f&2 else y)*8+(7-x if f&1 else x)] for y in range(8) for x in range(8)]
        r.append((f,tuple(q)))
    return r
bt,bm,bp,ba=rd('map')
NB=len(bt)//32
BP=[rgb(w) for w in bp]
base={}
for i in range(NB):
    for f,q in flips(pens(bt,i)):
        base.setdefault(q,(i,f))
def d2(a,b): return (a[0]-b[0])**2+(a[1]-b[1])**2+(a[2]-b[2])**2
TOL=int(sys.argv[1]) if len(sys.argv)>1 else 40
total_extra=0; report=[]
for n in ['map2','map3','map4','map5','map6']:
    t,m,p,a=rd(n)
    extra=[]; ex={}; err=0; words=[]
    allw=m+[a[k*2+1] for k in range(len(a)//2)]
    for ci,w in enumerate(allw):
        px=cellrgb(t,p,w)
        if ci<1120:
            b=cellrgb(bt,bp,bm[ci])
            if max(max(abs(u-v) for u,v in zip(c1,c2)) for c1,c2 in zip(b,px))<=TOL:
                words.append(bm[ci]); err+=sum(d2(c1,c2) for c1,c2 in zip(b,px)); continue
        if (w>>15)&1: pr=0x8000
        else: pr=0
        best=None
        for l in range(4):
            if l==0: pass
            q=[]; e=0
            for c in px:
                j=min(range(16),key=lambda k:d2(c,BP[l*16+k])) if True else 0
                q.append(j); e+=d2(c,BP[l*16+j])
            if best is None or e<best[0]: best=(e,l,tuple(q))
        e,l,q=best; err+=e
        if q in base:
            i,f=base[q]
        elif q in ex:
            i,f=ex[q]
        else:
            i=NB+len(extra); extra.append(q)
            for f2,q2 in flips(list(q)): ex.setdefault(q2,(i,f2))
            f=0
        # word: tile i shown with flip f means stored tile under flip f equals q
        words.append(pr|(l<<13)|((f&2)<<11)|((f&1)<<11)|i)
    nm=words[:1120]; na=words[1120:]
    open(O+n+'_map.bin','wb').write(struct.pack('>1120H',*nm))
    open(O+n+'_alt.bin','wb').write(struct.pack('>%dH'%len(a),*[v for k in range(len(a)//2) for v in (a[k*2],na[k])]))
    tb=b''
    for q in extra:
        for y in range(8):
            for x in range(0,8,2): tb+=bytes([(q[y*8+x]<<4)|q[y*8+x+1]])
    open(O+n+'_xtiles.bin','wb').write(tb)
    total_extra+=len(extra)
    report.append((n,len(extra),err/(1120*64)))
for r in report: print('%s extra tiles %d, mean sq err/px %.1f'%r)
print('base tiles',NB,'extra total',total_extra)
