#!/usr/bin/env python3
"""Decode captured CPS tile RAM/registers; reference-only, not a ROM asset bake."""
from pathlib import Path
import zipfile,argparse
from PIL import Image
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb

def render(vram,regs,gfx):
 def word(p,a):return int.from_bytes(p[a:a+2],'big')
 cache={};image=Image.new('RGB',(384,224))
 for y in range(224):
  for x in range(384):
   color=(0,0,0)
   for layer,size,base_reg,x_reg,y_reg,pal in [(2,16,4,16,18,0x800),(3,32,6,20,22,0xc00)]:
    xx=(x+64+word(regs,x_reg))%(64*size);yy=(y+16+word(regs,y_reg))%(64*size)
    col=xx//size;row=yy//size
    idx=(row&15)+(col<<4)+((row&48)<<6) if layer==2 else (row&7)+(col<<3)+((row&56)<<6)
    a=((word(regs,base_reg)<<8)&0x3ffff)+idx*4
    code=word(vram,a);attr=word(vram,a+2)
    code=0x4000+(code&0x1fff) if layer==2 else code&0x3fff
    if layer==3 and not 0xa00<=code<=0xdff:continue
    key=(size,code)
    if key not in cache:cache[key]=decode_tile(gfx,code,size)
    px=xx%size;py=yy%size
    pen=cache[key][size-1-py if attr&64 else py][size-1-px if attr&32 else px]
    if pen!=15:
     color=cps_rgb(word(vram,0x1c000+pal+(attr&31)*32+pen*2));break
   image.putpixel((x,y),color)
 return image
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('frame',type=int);a=ap.parse_args()
 with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
 root=Path('build/arcade-trace')
 im=render((root/f'intro-vram-{a.frame}.bin').read_bytes(),(root/f'intro-_cps_a_regs-{a.frame}.bin').read_bytes(),g)
 im.save(root/f'background-{a.frame}.png')
