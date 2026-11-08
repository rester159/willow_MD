#!/usr/bin/env python3
"""Original ROM color/geometry reference atlas; not a playable-stage claim."""
from pathlib import Path
import json,zipfile
from PIL import Image,ImageDraw
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import palette_words,cps_rgb
from cps1_gameplay_maps import gameplay_map

ROOT=Path(__file__).resolve().parents[1]
def main():
 p=assemble_maincpu(ROOT/'willow.zip')
 with zipfile.ZipFile(ROOT/'willow.zip') as z:g=assemble_region(z)
 out=ROOT/'build/visual-reference';out.mkdir(parents=True,exist_ok=True)
 # Do not emit the withdrawn same-map layer composite. Layer-aware reference
 # rendering lives in cps1_captured_background.py until the source map renderer
 # has been independently verified.
 manifest=[{**gameplay_map(p,stage),'runtime_verified':stage==0} for stage in range(8)]
 (out/'maps.json').write_text(json.dumps(manifest,indent=2)+'\n')
 sprites=Image.new('RGB',(768,3*160),(32,32,32));d=ImageDraw.Draw(sprites)
 actors=[('Willow',0,3,3,[0x33,0,3,6,9,0x36]),('Madmartigan (source)',1,4,4,[0x400,0x404,0x440,0x408,0x40c,0x444]),('Armored enemy',16,4,4,[0x140,0x144,0x148,0x14c,0x180,0x184])]
 for row,(name,group,w,h,codes) in enumerate(actors):
  pal=[cps_rgb(v|0xf000) for v in palette_words(p,'base',0,group)]
  for col,code in enumerate(codes):
   im=Image.new('RGB',(64,64),(32,32,32))
   for ty in range(h):
    for tx in range(w):
     tile=decode_tile(g,code+ty*16+tx,16)
     for y in range(16):
      for x in range(16):
       pen=tile[y][x]
       if pen!=15:im.putpixel((tx*16+x,ty*16+y),pal[pen])
   sprites.paste(im.resize((128,128),Image.Resampling.NEAREST),(col*128,row*160+32))
   d.text((col*128+2,row*160+18),f'{code:04X}',fill='white')
  d.text((2,row*160+2),name,fill='white')
 sprites.save(out/'characters-source.png')
 print('Generated foreground selector metadata and three source animation strips; scenery composite withdrawn')
if __name__=='__main__':main()
