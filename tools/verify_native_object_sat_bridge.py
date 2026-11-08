#!/usr/bin/env python3
"""Verify the CPS-object-to-SAT bridge using original graphic ROM pixels.

No emulator launch. Captured banks test geometry, not which bank was latched.
Each test gives the isolated object plane its own exact pen/palette atlas;
this does not establish a four-palette allocation for a complete game frame.
"""
from pathlib import Path
import ctypes as c
import hashlib
import json
import random
import struct
import subprocess
import zipfile
from cps1_gfx import assemble_region, decode_tile, genesis_tiles

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/cartridge-gap-audit'
OUT.mkdir(exist_ok=True,parents=True)
LIB=OUT/'object-sat-test.dylib'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-O2','-shared',
 '-fPIC',str(ROOT/'tools/test_native_object_sat_bridge.c'),'-o',str(LIB)],check=True)
class View(c.Structure):
 _fields_=[('left',c.c_uint16),('top',c.c_uint16),('width',c.c_uint16),('height',c.c_uint16),('flip',c.c_uint8)]
class Budget(c.Structure):
 _fields_=[('count',c.c_uint16),('sprites',c.c_uint8*224),('pixels',c.c_uint16*224)]
class Piece(c.Structure):
 _fields_=[('x',c.c_int16),('y',c.c_int16),('code',c.c_uint16),('source',c.c_uint16),('attribute',c.c_uint16)]
class Result(c.Structure):
 _fields_=[('status',c.c_uint8),('pieces',c.c_uint16),('records',c.c_uint16),('record',c.c_uint16),('column',c.c_uint16),('row',c.c_uint16)]
Asset=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_int16,c.c_int16,c.POINTER(c.c_uint16))
lib=c.CDLL(str(LIB))
lib.willowObjectSatTestBuild.argtypes=[c.POINTER(c.c_uint16),c.c_uint16,c.POINTER(View),c.POINTER(Budget),c.POINTER(Piece),c.c_uint16,Asset,c.c_void_p,c.POINTER(Result),c.POINTER(c.c_uint16)]
lib.willowObjectSatTestReserve.argtypes=[c.POINTER(Budget),c.c_int,c.c_uint,c.c_uint]
lib.willowObjectSatTestReserve.restype=c.c_uint
with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
cache={}
def tile(code):
 if code not in cache:cache[code]=decode_tile(gfx,code,16)
 return cache[code]
def reference(words,v):
 """Pixel-first blocked-object interpretation, independent of the C planner."""
 image=[-1]*(v.width*v.height)
 for at in range(0,len(words),4):
  x,y,code,a=words[at:at+4]
  if a&0xff00==0xff00:break
  if code>0x27ff:continue
  w=(((a>>8)&15)+1)*16;h=((a>>12)+1)*16
  # Visit output pixels, using inverse flips to select original source texel.
  for yy in range(h):
   rawy=((y+(yy//16)*16)&511)+(yy&15)
   py=(255-rawy if v.flip else rawy)-v.top
   if not 0<=py<v.height:continue
   sy=h-1-yy if a&64 else yy
   for xx in range(w):
    rawx=((x+(xx//16)*16)&511)+(xx&15)
    px=(511-rawx if v.flip else rawx)-v.left
    if not 0<=px<v.width:continue
    sx=w-1-xx if a&32 else xx
    child=(code&~15)+((code+sx//16)&15)+(sy//16)*16
    pen=tile(child)[sy&15][sx&15]
    pos=py*v.width+px
    if pen!=15 and image[pos]<0:image[pos]=(a&31)*16+pen
 return image
def run(words,flip=0,capacity=80,budget=None,missing=False,attribute_override=None):
 v=View(96,16,320,224,flip);b=budget or Budget();p=(Piece*80)();sat=(c.c_uint16*320)();r=Result()
 atlas={};palettes={};transparent=0
 @Asset
 def asset(_,code,a,x,y,out):
  nonlocal transparent
  if missing:return 0
  pixels=tile(code)
  if all(pen==15 for row in pixels for pen in row):transparent+=1;return 2
  pal=a&31
  if pal not in palettes:
   if len(palettes)==4:return 0
   palettes[pal]=len(palettes)
  if code not in atlas:
   if len(atlas)==80:return 0
   # Native tiles use pen 0 for transparent and 1..15 for CPS pens 0..14.
   raw=genesis_tiles([[0 if pen==15 else pen+1 for pen in row] for row in pixels])
   raw=raw[:32]+raw[64:96]+raw[32:64]+raw[96:]
   atlas[code]=(len(atlas)*4,raw)
  out[0]=atlas[code][0]|(palettes[pal]<<13)
  if attribute_override is not None:out[0]=attribute_override
  return 1
 array=(c.c_uint16*len(words))(*words)
 lib.willowObjectSatTestBuild(array,len(words),c.byref(v),c.byref(b),p,capacity,asset,None,c.byref(r),sat)
 assert list(array)==list(words), 'Object input bank changed'
 if r.status:return r,b,transparent
 # Interpret emitted Genesis SAT words and actual ROM-derived 4bpp tile bytes.
 patterns={base:raw for base,raw in atlas.values()};reverse={slot:pal for pal,slot in palettes.items()}
 image=[-1]*(320*224)
 for i in range(r.pieces):
  yw,szlink,attr,xw=sat[i*4:i*4+4]
  assert szlink>>8==5 and szlink&255==(i+1 if i+1<r.pieces else 0)
  raw=patterns[attr&0x7ff];pal=reverse[(attr>>13)&3];x=xw-128;y=yw-128
  for yy in range(16):
   py=y+yy
   if not 0<=py<224:continue
   sy=15-yy if attr&0x1000 else yy
   for xx in range(16):
    px=x+xx
    if not 0<=px<320:continue
    sx=15-xx if attr&0x800 else xx
    offset=((sx//8)*2+sy//8)*32+(sy&7)*4+(sx&7)//2
    pen=(raw[offset]>>(0 if sx&1 else 4))&15
    pos=py*320+px
    if pen and image[pos]<0:image[pos]=pal*16+pen-1
 assert image==reference(words,v),(r.record,r.pieces,flip)
 return r,b,transparent

captures=[];total_pixels=0;total_pieces=0
for f in sorted((ROOT/'build/arcade-trace').glob('intro-vram-*.bin')):
 raw=f.read_bytes()
 for bank in (0,0x8000):
  words=struct.unpack('>1024H',raw[bank:bank+0x800])
  for flip in (0,1):
   r,b,blank=run(words,flip)
   assert r.status==0,(f.name,bank,flip,r.status,r.record,r.column,r.row)
   captures.append({'file':str(f.relative_to(ROOT)),'bank':bank,'flip':flip,'pieces':r.pieces,'transparent':blank,'sha256':hashlib.sha256(raw).hexdigest()})
   total_pixels+=320*224;total_pieces+=r.pieces

# Deterministic noncaptured hardware-shape cases still use original ROM pixels.
# Small shapes stay within real H40 limits; large/offscreen cases exercise wrap.
rng=random.Random(0x57494c4c);shapes=0;rejections=0
for n in range(512):
 nx=rng.randrange(1,7);ny=rng.randrange(1,7);a=((ny-1)<<12)|((nx-1)<<8)|rng.choice((0,32,64,96))|rng.randrange(4)
 words=[rng.choice((82,95,96,112,390,410,500,511)),rng.choice((0,8,16,32,220,239,500,511)),rng.randrange(0x2700),a,0,0,0,0xff00]
 r,_,_=run(words,n&1)
 if r.status==0:shapes+=1;total_pixels+=320*224
 else:assert r.status in (3,4,5);rejections+=1
# Proven overlapping ROM sprites verify front-first order and transparency.
for flip in (0,1):
 words=[140,90,0x11,0,145,94,0x31,1,160,100,0x71,2,0,0,0,0xff00]
 r,_,_=run(words,flip);assert r.status==0;total_pixels+=320*224

# Limits are errors, never implicit truncation, including preexisting HUD cost.
opaque=next(code for code in range(0x100) if any(pen!=15 for row in tile(code) for pen in row))
words=[140,90,opaque,0,0,0,0,0xff00]
assert run(words,missing=True)[0].status==2
assert run(words,attribute_override=0x0800)[0].status==2
assert run(words,attribute_override=2045)[0].status==2
assert run(words,capacity=0)[0].status==3
assert run(words,capacity=81)[0].status==1
assert run(words,flip=2)[0].status==1
assert run([0]*1028)[0].status==1
assert run([0]*3)[0].status==1
b=Budget();b.count=80;assert run(words,budget=b)[0].status==3
b=Budget();b.count=81;assert run([0,0,0,0xff00],budget=b)[0].status==3
b=Budget();b.sprites[80]=21;assert run([0,0,0,0xff00],budget=b)[0].status==4
b=Budget();b.pixels[80]=321;assert run([0,0,0,0xff00],budget=b)[0].status==5
b=Budget();b.sprites[80]=20;assert run(words,budget=b)[0].status==4
b=Budget();b.pixels[80]=320;assert run(words,budget=b)[0].status==5
for count in range(11):
 b=Budget()
 for _ in range(count):assert lib.willowObjectSatTestReserve(c.byref(b),80,32,16)==0
 expected=5 if count==10 else 0
 assert lib.willowObjectSatTestReserve(c.byref(b),80,32,16)==expected
b=Budget()
for _ in range(10):assert lib.willowObjectSatTestReserve(c.byref(b),80,32,16)==0
before=bytes(b);assert lib.willowObjectSatTestReserve(c.byref(b),80,16,16)==5;assert bytes(b)==before
# End-marker excludes following records; unavailable PAL base codes are skipped.
assert run([0,0,0,0xff00,140,90,opaque,0])[0].pieces==0
assert run([140,90,0x2800,0])[0].pieces==0
# A lower object must never replace the earlier object's opaque pixels.
# Attribute marker 0xFFxx takes precedence over the nominal 16x16 block shape.
for flags in (0,32,64,96):
 for code in (0x000f,0x001f,0x27ff):
  assert run([100,30,code,0x1100|flags,0,0,0,0xff00])[0].status==0
# Two source blocks on the same line exceed H40 admission; no truncated success.
assert run([100,40,opaque,0x0f00,100,40,opaque,0x0f00])[0].status==4
report={'captured_banks_and_flip_cases':len(captures),'shape_cases_passed':shapes,'shape_limit_rejections':rejections,'compared_pixels':total_pixels,'captured_pieces':total_pieces,'captures':captures,'scope':'Host compiled C consumer plus independent software SAT pixel reconstruction from original ROM graphics; no target cartridge, latch timing, complete-frame palette allocation or FPS claim.'}
(OUT/'object-sat-evidence.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k!='captures'})
