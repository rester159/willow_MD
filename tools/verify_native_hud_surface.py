#!/usr/bin/env python3
"""Compare C-composed 320px tiles to independent ROM pixel-center sampling."""
from pathlib import Path
import json,zipfile
from PIL import Image,ImageDraw
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import palette_words,cps_rgb
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
a=json.loads(Path('build/native-hud-assets.json').read_text())
pal=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in a['palette']]
colors={};pens_cache={};checks=0;preview=None
for frame in (1588,1590,1600):
 source=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 for flips in (0,1):
  packed=Path(f'build/hud-surface-{frame}-{flips}.bin').read_bytes();assert len(packed)==7680
  rendered=Image.new('RGB',(320,224),'black');original=Image.new('RGB',(384,224),'black')
  for row_index,row in enumerate((2,3,26,28,29,30)):
   for col in range(48):
    at=col*128+row*4;code=int.from_bytes(source[at:at+2],'big');attr=int.from_bytes(source[at+2:at+4],'big');group=attr&31
    if flips:attr^=(32 if col&1 else 0)|(64 if col&2 else 0)
    key=(code,col&1)
    if key not in pens_cache:pens_cache[key]=decode_tile(g,code,8,col&1)
    pens=pens_cache[key]
    if group not in colors:colors[group]=[cps_rgb(w|0xf000) for w in palette_words(p,'scroll1',0,group)]
    for y in range(8):
     for x in range(8):
      pen=pens[7-y if attr&64 else y][7-x if attr&32 else x]
      if row<30 and pen!=15:original.putpixel((col*8+x,(row-2)*8+y),colors[group][pen])
   for y in range(8):
    for x in range(320):
     sx=(6*x+3)//5;col=sx//8;at=col*128+row*4
     code=int.from_bytes(source[at:at+2],'big');attr=int.from_bytes(source[at+2:at+4],'big');group=attr&31
     if flips:attr^=(32 if col&1 else 0)|(64 if col&2 else 0)
     pen=pens_cache[code,col&1][7-y if attr&64 else y][7-(sx&7) if attr&32 else sx&7]
     want=0 if pen==15 else 1+min(range(15),key=lambda n:color_error(colors[group][pen],pal[n+1]))
     byte=packed[(row_index*40+x//8)*32+y*4+(x&7)//2];got=(byte>>(0 if x&1 else 4))&15
     assert got==want,(frame,flips,row,x,y,got,want)
     if row<30 and got:rendered.putpixel((x,(row-2)*8+y),pal[got])
     checks+=1
  if frame==1590 and not flips:
   preview=Image.new('RGB',(704,248),'#202020');draw=ImageDraw.Draw(preview)
   draw.text((8,5),'Arcade HUD: 384 pixels',fill='white');draw.text((392,5),'Native composition: 320 pixels',fill='white')
   preview.paste(original,(0,24));preview.paste(rendered,(384,24))
preview.save('build/native-hud-surface-preview.png')
report={'pixel_comparisons':checks,'mismatches':0,'frames':3,'flip_variants':2,'surface_ram_bytes':1188,
 'projection_rom_bytes':a['projected_rom_bytes'],'scope':'C tile composition versus independent ROM sampling; all six stored rows including offscreen row 30. Native VDP upload/cache integration and runtime frame budget remain pending.'}
Path('build/native-hud-surface-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
