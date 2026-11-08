#!/usr/bin/env python3
"""Check opaque intro-actor pixels from original sprite blocks and palette RAM."""
from pathlib import Path
import json,zipfile,csv
from PIL import Image
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb
from cps1_intro_actor_render import word,emit,actor_trace
from cps1_program import assemble_maincpu
with zipfile.ZipFile('willow.zip') as z:gfx=assemble_region(z)
cache={};report=[]
program=assemble_maincpu(Path('willow.zip'))
actors={(r['frame'],r['slot']):r for r in actor_trace()}
covered_sprites={}
items=json.loads(Path('build/arcade-intro-actor-render.json').read_text())
items.sort(key=lambda r:(r['frame'],[1,2,0].index(r['slot'])))
for item in items:
 # Object scanout trails the emitted source record by two frames.
 actor=actors[item['frame']-2,item['slot']]
 r=emit(program,actor,0,actor['camera'])
 if r is None:continue
 frame=item['frame'];root=Path('build/arcade-trace');ram=(root/f'intro-vram-{frame}.bin').read_bytes()
 actual=Image.open(root/f'natural-intro-{frame}.png').convert('RGB');count=bad=0
 # Source Scroll1 is drawn in front of these actors.
 def covered(x,y):
  col=x//8;row=(y+16)//8;a=0x10000+(row&31)*4+col*128+((row&32)<<8)
  code=word(ram,a);attr=word(ram,a+2);key=('text',code,col&1)
  if key not in cache:cache[key]=decode_tile(gfx,code,8,col&1)
  return cache[key][(y+16)%8][x%8]!=15
 for ty in range(r['height']):
  for tx in range(r['width']):
   code=(r['code']&~15)+((r['code']+tx)&15)+ty*16
   if code not in cache:cache[code]=decode_tile(gfx,code,16)
   for py in range(16):
    for px in range(16):
     pen=cache[code][py][px]
     if pen==15:continue
     x=tx*16+px;y=ty*16+py
     if r['attribute']&32:x=r['width']*16-1-x
     if r['attribute']&64:y=r['height']*16-1-y
     x+=r['x']-64;y+=r['y']-16
     if not(0<=x<384 and 0<=y<224):continue
     mask=covered_sprites.setdefault(frame,set())
     if (x,y) in mask or covered(x,y):continue
     mask.add((x,y))
     c=cps_rgb(word(ram,0x1c000+(r['attribute']&31)*32+pen*2))
     count+=1;bad+=actual.getpixel((x,y))!=c
 report.append({'frame':frame,'slot':item['slot'],'opaque_pixels':count,'object_scanout_lag':2,'mismatches':bad});print(report[-1])
Path('build/arcade-intro-actor-pixels.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(x['mismatches']==0 for x in report)
