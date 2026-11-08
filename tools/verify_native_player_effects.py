#!/usr/bin/env python3
"""Independently reconstruct every packed resident player palette variant."""
from pathlib import Path
import json,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb
from cps1_intro_actor_render import emit
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
a=json.loads(Path('build/intro-sprite-assets.json').read_text());frame=a['frames'][-1]
hud=json.loads(Path('build/native-hud-assets.json').read_text())
packed=Path('native/intro_probe/res/player_effects.bin').read_bytes()
pose=emit(p,{'active':1,'x':0,'y':0,'animation':0xd15a},0,0)
source=[[15]*48 for _ in range(48)]
for ty in range(3):
 for tx in range(3):
  tile=decode_tile(g,(pose['code']&~15)+((pose['code']+tx)&15)+ty*16,16)
  for y in range(16):source[ty*16+y][tx*16:tx*16+16]=tile[y]
if pose['attribute']&32:source=[row[::-1] for row in source]
if pose['attribute']&64:source=source[::-1]
offsets={v:n*128 for n,v in enumerate(hud['player_offsets'])}
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in hud['palette'][1:]]
size=len(offsets)*128;assert len(packed)==48*size
checks=0
for kind,address in enumerate((0xe5080,0xe5000,0xe5040)):
 for brightness in range(16):
  actual=[[0]*48 for _ in range(48)];data=packed[(kind*16+brightness)*size:(kind*16+brightness+1)*size]
  for x,y,w,h,offset in a['chunks'][frame[3]:frame[3]+frame[4]]:
   for yy in range(h*8):
    for xx in range(w*8):
     at=offsets[offset]+((xx//8)*h+yy//8)*32+yy%8*4+(xx%8)//2
     actual[y+yy][x+xx]=(data[at]>>(0 if xx&1 else 4))&15
  colors=[cps_rgb(int.from_bytes(p[address+n*2:address+n*2+2],'big')|(brightness<<12)) for n in range(16)]
  for y in range(48):
   for x in range(48):
    pen=source[y][x]
    expected=0 if pen==15 else 1+min(range(15),key=lambda n:color_error(colors[pen],palette[n]))
    assert actual[y][x]==expected,(kind,brightness,x,y)
    checks+=1
report={'variants':48,'pixel_comparisons':checks,'mismatches':0,'scope':'Original source pixels and palettes, fitted to the fixed HUD palette; resident pose only.'}
Path('build/native-player-effects-parity.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
