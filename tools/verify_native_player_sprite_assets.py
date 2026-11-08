#!/usr/bin/env python3
"""Check packed gameplay sprite blocks against original decoded CPS pens."""
from pathlib import Path
import json,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
a=json.loads(Path('build/native-player-sprites.json').read_text())
hud=json.loads(Path('build/native-hud-assets.json').read_text())
packed=Path('native/intro_probe/res/player_sprites.bin').read_bytes();assert len(packed)==a['blocks']*128
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in hud['palette'][1:]]
raw=[decode_tile(g,code,16) for code in a['source_codes']];checks=0
for kind,base in enumerate((0xe5080,0xe5000,0xe5040)):
 for brightness in range(16):
  colors=[cps_rgb(int.from_bytes(p[base+n*2:base+n*2+2],'big')|(brightness<<12)) for n in range(16)]
  lookup=[1+min(range(15),key=lambda i:color_error(c,palette[i])) for c in colors];lookup[15]=0
  for n,source in enumerate(raw):
   offset=a['variants'][kind*16+brightness][n]*128
   for y in range(16):
    for x in range(16):
     at=offset+((x//8)*2+y//8)*32+(y&7)*4+(x&7)//2
     actual=(packed[at]>>(0 if x&1 else 4))&15
     assert actual==lookup[source[y][x]],(kind,brightness,a['source_codes'][n],x,y)
     checks+=1
report={'source_tiles':len(raw),'variants':48,'pixel_checks':checks,'mismatches':0,'scope':'Original source pens and palettes, including opacity, for every packed Willow gameplay block.'}
Path('build/native-player-sprite-assets-parity.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
