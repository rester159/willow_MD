#!/usr/bin/env python3
"""Check packed HUD pixels against ROM pens and source capture coverage."""
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
blob=Path('native/intro_probe/res/hud.bin').read_bytes()
assert len(blob)==a['patterns']*32
pal=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in a['palette']]
source_colors={group:[cps_rgb(w|0xf000) for w in palette_words(p,'scroll1',0,group)] for _,group,_,_ in a['records']}
keys={tuple(r[:2]):r[2:] for r in a['records']}
checks=0
for (code,group),patterns in keys.items():
 for layout,pattern in enumerate(patterns):
  pens=decode_tile(g,code,8,layout)
  for y in range(8):
   for x in range(8):
    v=blob[pattern*32+y*4+x//2];v=(v>>(0 if x&1 else 4))&15
    pen=pens[y][x]
    assert (v==0)==(pen==15),(code,group,layout,x,y)
    if pen!=15:
     rgb=source_colors[group][pen]
     assert color_error(rgb,pal[v])==min(color_error(rgb,q) for q in pal[1:])
    checks+=1
# Captured CPS text memory is verification input only, never generated ROM data.
images=[];captured=0;error=0;opaque=0
for frame in (1588,1590,1600):
 ram=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 panels=[Image.new('RGB',(384,224),(0,0,0)) for _ in range(2)]
 for row in (2,3,28,29):
  for col in range(48):
   offset=col*128+row*4
   code=int.from_bytes(ram[offset:offset+2],'big');attr=int.from_bytes(ram[offset+2:offset+4],'big');group=attr&31
   pens=decode_tile(g,code,8,col&1)
   if all(v==15 for line in pens for v in line):continue
   assert (code,group) in keys,(frame,hex(code),group,col,row)
   pattern=keys[code,group][col&1]
   for y in range(8):
    for x in range(8):
     sx=7-x if attr&32 else x;sy=7-y if attr&64 else y;pen=pens[sy][sx]
     if pen==15:continue
     v=blob[pattern*32+sy*4+sx//2];v=(v>>(0 if sx&1 else 4))&15
     rgb=source_colors[group][pen];mapped=pal[v]
     panels[0].putpixel((col*8+x,(row-2)*8+y),rgb)
     panels[1].putpixel((col*8+x,(row-2)*8+y),mapped)
     error+=sum((b-c)**2 for b,c in zip(rgb,mapped));opaque+=1
   captured+=1
 if frame==1590:images=panels
preview=Image.new('RGB',(768,248),'#202020');draw=ImageDraw.Draw(preview)
draw.text((8,5),'Arcade ROM colors',fill='white');draw.text((392,5),'Candidate Genesis palette (full 384px HUD)',fill='white')
preview.paste(images[0],(0,24));preview.paste(images[1],(384,24));preview.save('build/native-hud-palette-preview.png')
report={'patterns':a['patterns'],'keys':len(keys),'pixel_checks':checks,'capture_frames':[1588,1590,1600],
 'captured_visible_cells':captured,'captured_opaque_pixels':opaque,'capture_rgb_rmse':(error/opaque/3)**.5,
 'scope':'Packed source geometry/opacity, palette nearest-color mapping and coverage of captured HUD rows; no native renderer integration or final 320px layout claim.'}
Path('build/native-hud-asset-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
