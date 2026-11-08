#!/usr/bin/env python3
"""Convert original blocked intro actor frames into Genesis hardware sprites."""
from pathlib import Path
from collections import Counter
import zipfile,json
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb,palette_words
from native_palette_fit import fit_palette,color_error
from cps1_intro_actor_render import emit
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
source_palettes=[[cps_rgb(w|0xf000) for w in palette_words(p,'base',0,group)] for group in range(32)]
frames=[];hist=Counter()
for a in [0x325bc,0x325c8,0x325d4,0x32aa2,0x32aae,0x32aba,0x32aca,0x32ad6,0xd15a]:
 r=emit(p,{'active':1,'x':0,'y':0,'animation':a},0,0)
 w,h=r['width']*16,r['height']*16;pix=[[None]*w for _ in range(h)]
 for ty in range(r['height']):
  for tx in range(r['width']):
   code=(r['code']&~15)+((r['code']+tx)&15)+ty*16
   tile=decode_tile(g,code,16)
   for y in range(16):
    for x in range(16):
     pen=tile[y][x]
     if pen==15:continue
     xx=tx*16+x;yy=ty*16+y
     if r['attribute']&32:xx=w-1-xx
     if r['attribute']&64:yy=h-1-yy
     rgb=source_palettes[r['attribute']&31][pen]
     pix[yy][xx]=rgb
     if a!=0xd15a:hist[rgb]+=1
 frames.append({'address':a,'width':w,'height':h,'x':r['x']-64,'y':r['y']-256,'pixels':pix})
palette=fit_palette(hist);lookup={c:1+min(range(15),key=lambda i:color_error(c,palette[i])) for c in hist}
pal0_words=json.loads(Path('build/intro-font.json').read_text())['genesis_palette']
pal0=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in pal0_words[1:]]
player_lookup={c:1+min(range(15),key=lambda i:color_error(c,pal0[i])) for row in frames[-1]['pixels'] for c in row if c is not None}
blob=bytearray();chunks=[];records=[];errors=[]
for f in frames:
 first=len(chunks);start=len(blob)//32
 mapping=player_lookup if f['address']==0xd15a else lookup
 for yy in range(0,f['height'],16):
  for xx in range(0,f['width'],16):
   w=min(16,f['width']-xx)//8;h=min(16,f['height']-yy)//8
   chunks.append([xx,yy,w,h,len(blob)//32-start])
   # Genesis hardware sprite tile order is column-major.
   for tx in range(w):
    for ty in range(h):
     for py in range(8):
      for px in range(0,8,2):
       pair=[f['pixels'][yy+ty*8+py][xx+tx*8+px+i] for i in range(2)]
       pens=[0 if c is None else mapping[c] for c in pair]
       blob.append(pens[0]*16+pens[1])
 records.append([f['address'],start,len(blob)//32-start,first,len(chunks)-first,f['x'],f['y'],0 if f['address']==0xd15a else 1])
 for row in f['pixels']:
  for c in row:
   if c is not None and f['address']!=0xd15a:errors.append(sum((a-b)**2 for a,b in zip(c,palette[lookup[c]-1])))
# Keep every distinct nonblank 16x16 block resident; animation changes only SAT.
unique=[];indices={};resident_chunks=[]
for r in records:
 old_first,old_count=r[3],r[4];r[3]=len(resident_chunks)
 for c in chunks[old_first:old_first+old_count]:
  block=bytes(blob[(r[1]+c[4])*32:(r[1]+c[4]+4)*32])
  if not any(block):continue
  if block not in indices:indices[block]=len(unique);unique.append(block)
  resident_chunks.append(c[:4]+[indices[block]*4])
 r[4]=len(resident_chunks)-r[3]
blob=bytearray(b''.join(unique));chunks=resident_chunks
assert len(unique)<=84, 'Resident sprite blocks exceed reserved VRAM'
words=[0]+[sum((v//34)<<s for v,s in zip(c,(1,5,9))) for c in palette]
lines=['#define INTRO_ACTOR_BLOCKS %d'%len(unique),'/* Converted original frames; palette fitting is explicit. */','typedef struct {u32 address;u16 offset,count,first,chunks;s16 x,y;u16 palette;} IntroSpriteFrame;','typedef struct {u16 x,y,w,h,offset;} IntroSpriteChunk;','static const u16 intro_actor_palette[16]={'+','.join(map(str,words))+'};','static const IntroSpriteFrame intro_sprite_frames[]={'+','.join('{'+','.join(map(str,r))+'}' for r in records)+'};','static const IntroSpriteChunk intro_sprite_chunks[]={'+','.join('{'+','.join(map(str,r))+'}' for r in chunks)+'};']
Path('native/intro_probe/res/actors.bin').write_bytes(blob)
Path('native/intro_probe/src/actor_sprites.h').write_text('\n'.join(lines)+'\n')
manifest={'frames':records,'chunks':chunks,'palette':words,'resident_blocks':len(unique),'source_unique_colors':len(hist),'source_rgb_rmse':(sum(errors)/len(errors)/3)**.5,'max_tiles_per_actor':[max(r[2] for r in records[:3]),max(r[2] for r in records[3:8]),records[8][2]]}
Path('build/intro-sprite-assets.json').write_text(json.dumps(manifest,indent=2)+'\n');print({k:v for k,v in manifest.items() if k not in ('frames','chunks')})
