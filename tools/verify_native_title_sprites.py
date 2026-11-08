#!/usr/bin/env python3
"""Check stored Genesis pixels and geometry against ROM and source CPU output."""
from pathlib import Path
import csv,json,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'));w=lambda a:int.from_bytes(p[a:a+2],'big')
report=json.loads(Path('build/native-title-sprites.json').read_text())
blob=Path('native/intro_probe/res/title_sprites.bin').read_bytes()
assert len(blob)==report['unique_blocks']*128
palette=[tuple(((word>>s)&7)*34 for s in (1,5,9)) for word in report['palette']]
source={}
for row in csv.DictReader(open('build/title-source-sprites.csv')):
 row={k:int(v) for k,v in row.items()}
 source.setdefault(row['animation'],{}).setdefault(row['piece'],(row['x'],row['y'],row['code'],row['attribute']))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
checks=0
for frame in report['frames']:
 address=frame['address'];assert address in source
 assert len(frame['pieces'])==len(source[address])
 for i,(dx,dy,code,attribute,block) in enumerate(frame['pieces']):
  assert ((256+dx)&511,(192+dy)&511,code,attribute)==source[address][i]
  pixels=decode_tile(g,code,16)
  for y in range(16):
   for x in range(16):
    # Independently address column-major 8x8 Genesis tiles and packed nibbles.
    at=block*128+(x//8*2+y//8)*32+(y%8)*4+(x%8)//2
    pen=(blob[at]>>(0 if x&1 else 4))&15
    original=pixels[y][x]
    if original==15:assert pen==0
    else:
     rgb=cps_rgb(w(0xe5080+(attribute&31)*32+original*2)|0xf000)
     assert pen and color_error(rgb,palette[pen])==min(color_error(rgb,c) for c in palette[1:])
    checks+=1
print(f'Native opening actor: {len(report["frames"])} frames match source CPU pieces; {checks} stored pixels match transparent pens and nearest fitted RGB')
