#!/usr/bin/env python3
"""Compare isolated native pose scanouts with original CPS tile pixels."""
from pathlib import Path
import json,zipfile
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_intro_actor_render import emit
from cps1_palette import palette_words,cps_rgb
from native_palette_fit import color_error
from verify_native_intro_pixels import render,add_actors
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
a=json.loads(Path('build/native-player-sprites.json').read_text())
hud=json.loads(Path('build/native-hud-assets.json').read_text())
pal=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in hud['palette'][1:]]
display=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in pal]
colors=[cps_rgb(w|0xf000) for w in palette_words(p,'base',0,0)]
mapping=[display[min(range(15),key=lambda n:color_error(color,pal[n]))] for color in colors]
background=add_actors(render(898),897,slots=(0,));records=[]
for n,f in enumerate(a['frames']):
 tick=933+n*4;expected=background.copy()
 o=emit(p,{'active':1,'x':192,'y':72,'animation':f[0]},0,0)
 for ty in range(o['height']):
  for tx in range(o['width']):
   tile=decode_tile(g,(o['code']&~15)+((o['code']+tx)&15)+ty*16,16)
   for y in range(16):
    for x in range(16):
     pen=tile[y][x]
     if pen==15:continue
     xx=tx*16+x;yy=ty*16+y
     if o['attribute']&32:xx=o['width']*16-1-xx
     if o['attribute']&64:yy=o['height']*16-1-yy
     xx+=o['x']-96;yy+=o['y']-16
     if 0<=xx<320 and 0<=yy<224:expected[yy*320+xx]=mapping[pen]
 expected=add_actors(expected,897,slots=(1,))
 actual=list(Image.open(f'build/player-pose-probe-{tick}.ppm').convert('RGB').getdata())
 # Exclude HUD rows (its clock runs normally during this longer probe).
 indices=[y*320+x for y in range(16,192) for x in range(320)]
 bad=[i for i in indices if actual[i]!=expected[i]]
 records.append({'tick':tick,'animation':f[0],'pixels':len(indices),'mismatches':len(bad),'examples':[(i%320,i//320,actual[i],expected[i]) for i in bad[:3]]})
 if bad:print(records[-1])
result={'poses':len(records),'pixel_comparisons':sum(r['pixels'] for r in records),'frames':records,'scope':'Injected animation records isolate geometry/rendering at a fixed source position; this does not establish active gameplay scheduling.'}
Path('build/native-player-pose-pixels.json').write_text(json.dumps(result,indent=2)+'\n')
assert not any(r['mismatches'] for r in records)
print(f'Native Willow poses: {len(records)} poses, {result["pixel_comparisons"]} pixels, zero mismatches')
