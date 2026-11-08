#!/usr/bin/env python3
"""Check late player pixels, including source flips and transparent blocks."""
from pathlib import Path
import argparse, json, zipfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--hud",action="store_true",help="Verify the gameplay HUD palette version of the resident pose")
args=parser.parse_args()
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb,palette_words
from cps1_intro_actor_render import emit
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
base=json.loads(Path('build/intro-sprite-assets.json').read_text())
assets=json.loads(Path('build/native-hud-assets.json' if args.hud else 'build/intro-overlay-assets.json').read_text())
data=Path('native/intro_probe/res/hud_player.bin' if args.hud else 'native/intro_probe/res/overlay_player.bin').read_bytes()
offsets={offset:i*128 for i,offset in enumerate(assets['player_offsets'])}
frame=base['frames'][-1];actual=[[0]*48 for _ in range(48)]
for x,y,w,h,offset in base['chunks'][frame[3]:frame[3]+frame[4]]:
 for yy in range(h*8):
  for xx in range(w*8):
   at=offsets[offset]+((xx//8)*h+yy//8)*32+yy%8*4+(xx%8)//2
   b=data[at];actual[y+yy][x+xx]=b&15 if xx&1 else b>>4
pose=emit(p,{'active':1,'x':0,'y':0,'animation':0xd15a},0,0)
source=[[15]*48 for _ in range(48)]
for ty in range(3):
 for tx in range(3):
  tile=decode_tile(g,(pose['code']&~15)+((pose['code']+tx)&15)+ty*16,16)
  for y in range(16):
   source[ty*16+y][tx*16:tx*16+16]=tile[y]
if pose['attribute']&32:source=[row[::-1] for row in source]
if pose['attribute']&64:source=source[::-1]
source_palette=[cps_rgb(w|0xf000) for w in palette_words(p,'base',0,pose['attribute']&31)]
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in assets['palette'][1:]]
opaque=0
for y in range(48):
 for x in range(48):
  pen=source[y][x]
  expected=0 if pen==15 else 1+min(range(15),key=lambda i:color_error(source_palette[pen],palette[i]))
  assert actual[y][x]==expected,(x,y,actual[y][x],expected)
  opaque+=pen!=15
report={'pixels':2304,'opaque_pixels':opaque,'source_attribute':pose['attribute'],'mismatches':0}
Path('build/native-hud-player-parity.json' if args.hud else 'build/intro-overlay-player-parity.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'Late player: all 2304 source-derived pixels match, including {opaque} opaque pixels and source horizontal flip')
