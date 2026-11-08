#!/usr/bin/env python3
"""Verify Scroll1 decoding against original arcade screenshots (no Genesis assets)."""
from pathlib import Path
import zipfile,json
from PIL import Image
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb
with zipfile.ZipFile('willow.zip') as z:gfx=assemble_region(z)
cache={};report=[]
for frame in [780,810,840,870,900,930,960,990,1020]:
 root=Path('build/arcade-trace');ram=(root/f'intro-vram-{frame}.bin').read_bytes()
 actual=Image.open(root/f'natural-intro-{frame}.png').convert('RGB')
 count=bad=0
 for row in (13,14):
  for col in range(4,44):
   at=0x10000+col*128+row*4
   code=int.from_bytes(ram[at:at+2],'big');attr=int.from_bytes(ram[at+2:at+4],'big')
   key=code,col&1
   if key not in cache:cache[key]=decode_tile(gfx,code,8,col&1)
   for y,pens in enumerate(cache[key]):
    for x,pen in enumerate(pens):
     if pen==15:continue
     a=0x1c400+(attr&31)*32+pen*2
     color=cps_rgb(int.from_bytes(ram[a:a+2],'big'))
     count+=1;bad+=actual.getpixel((col*8+x,row*8-16+y))!=color
 report.append({'frame':frame,'opaque_glyph_pixels':count,'mismatches':bad})
 print(report[-1])
assert all(r['mismatches']==0 for r in report)
Path('build/arcade-intro-glyph-pixels.json').write_text(json.dumps(report,indent=2)+'\n')
