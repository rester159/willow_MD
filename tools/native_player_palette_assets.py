#!/usr/bin/env python3
"""Pack resident Willow colors from the three original health palettes."""
from pathlib import Path
import json,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb
from cps1_intro_actor_render import emit
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
a=json.loads(Path('build/intro-sprite-assets.json').read_text());frame=a['frames'][-1]
hud=json.loads(Path('build/native-hud-assets.json').read_text())
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in hud['palette'][1:]]
pose=emit(p,{'active':1,'x':0,'y':0,'animation':frame[0]},0,0)
assert pose['width']==pose['height']==3 and pose['code']==0x33
source=[[15]*48 for _ in range(48)]
for ty in range(3):
 for tx in range(3):
  tile=decode_tile(g,0x33+tx+ty*16,16)
  for y in range(16):
   for x in range(16):
    xx=tx*16+x;yy=ty*16+y
    if pose['attribute']&32:xx=47-xx
    if pose['attribute']&64:yy=47-yy
    source[yy][xx]=tile[y][x]
addresses=[0xe5080,0xe5000,0xe5040]
words=[[int.from_bytes(p[at+n*2:at+n*2+2],'big') for n in range(16)] for at in addresses]
variants=[]
for colors in words:
 for brightness in range(16):
  mapped=[1+min(range(15),key=lambda n:color_error(cps_rgb(w|brightness<<12),palette[n])) for w in colors]
  mapped[15]=0;blocks={}
  for x,y,w,h,offset in a['chunks'][frame[3]:frame[3]+frame[4]]:
   assert w==h==2
   data=genesis_tiles([[mapped[v] for v in row[x:x+16]] for row in source[y:y+16]])
   data=data[:32]+data[64:96]+data[32:64]+data[96:]
   if offset in blocks:assert blocks[offset]==data
   blocks[offset]=data
  assert list(blocks)==hud['player_offsets']
  variants.append(b''.join(blocks.values()))
assert variants[15]==Path('native/intro_probe/res/hud_player.bin').read_bytes()
Path('native/intro_probe/res/player_effects.bin').write_bytes(b''.join(variants))
Path('native/intro_probe/src/player_effects.h').write_text('/* Original C240 palette sources; renderer supports the resident Willow pose. */\n'
 f'#define NATIVE_PLAYER_EFFECT_BYTES {len(variants[0])}\n'
 'static const u16 native_player_effect_words[3][16]={'+','.join('{'+','.join(map(str,row))+'}' for row in words)+'};\n')
report={'variants':48,'source_addresses':addresses,'brightness_levels':16,'bytes_per_variant':len(variants[0]),'player_offsets':hud['player_offsets'],'scope':'Resident Willow pose mapped to the existing HUD palette; full moving-player graphics are separate.'}
Path('build/native-player-effects.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
