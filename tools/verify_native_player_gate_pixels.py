#!/usr/bin/env python3
"""Check the visible player against original pose data across the native gate.

This deliberately does not claim complete post-intro scene parity: gameplay
actor handlers remain incomplete. Only original player pixels not obscured by the
diagnostic's held scripted character are compared, using the fitted palette.
"""
from pathlib import Path
import argparse,csv,json,zipfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--palette-probe",action="store_true")
parser.add_argument("--damage-probe",action="store_true")
args=parser.parse_args()
probe=args.palette_probe or args.damage_probe
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_intro_actor_render import emit
from cps1_palette import cps_rgb,palette_words
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
trace={int(r['frame']):bytes.fromhex(r['record']) for r in csv.DictReader(open('build/arcade-trace/actor-native/handoff-player.csv'))}
scripted=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(open('build/arcade-trace/actor-native/actors.csv')) if r['frame']=='1587' and r['slot']=='1'][0]
def source_pixels(actor):
 o=emit(p,actor,0,0);w=o['width']*16;h=o['height']*16;result={}
 for ty in range(o['height']):
  for tx in range(o['width']):
   tile=decode_tile(g,(o['code']&~15)+((o['code']+tx)&15)+ty*16,16)
   for y in range(16):
    for x in range(16):
     pen=tile[y][x]
     if pen==15:continue
     xx=tx*16+x;yy=ty*16+y
     if o['attribute']&32:xx=w-1-xx
     if o['attribute']&64:yy=h-1-yy
     result[o['x']-64-32+xx,o['y']-16+yy]=pen
 return o,result
_,occlusion=source_pixels(scripted)
assets=json.loads(Path('build/native-hud-assets.json').read_text())
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in assets['palette'][1:]]
display=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in palette]
report=[]
for tick in (range(931,951) if probe else (899,900,901,915,916,920,930)):
 raw=trace[min(1620,690+tick-2)]
 actor={'active':raw[0],'x':int.from_bytes(raw[6:8],'big'),'y':int.from_bytes(raw[10:12],'big'),'animation':int.from_bytes(raw[20:24],'big')}
 o,pixels=source_pixels(actor)
 colors=[cps_rgb(w|0xf000) for w in palette_words(p,'base',0,o['attribute']&31)]
 if probe and 931<=tick-1<=942 and not(((tick-1-931)//4)&1):
  address=0xe5000 if args.damage_probe else 0xe5040
  colors=[cps_rgb(int.from_bytes(p[address+n*2:address+n*2+2],'big')|0xf000) for n in range(16)]
 name=('player-damage-probe' if args.damage_probe else 'player-palette-probe') if probe else 'intro-probe'
 actual=Image.open(f'build/{name}-{tick}.ppm').convert('RGB');count=0
 for xy,pen in pixels.items():
  if xy in occlusion or not(0<=xy[0]<320 and 0<=xy[1]<224):continue
  index=min(range(15),key=lambda i:color_error(colors[pen],palette[i]))
  assert actual.getpixel(xy)==display[index],(tick,xy,actual.getpixel(xy),display[index])
  count+=1
 assert count>100
 report.append({'tick':tick,'source_animation':actor['animation'],'visible_player_pixels':count,'mismatches':0})
result={'frames':report,'pixels':sum(r['visible_player_pixels'] for r in report),'scope':'Player-only comparison; held scripted-character occlusion excluded; converted palette, not original CPS RGB'}
Path(('build/native-player-damage-pixels.json' if args.damage_probe else 'build/native-player-palette-pixels.json') if probe else 'build/native-player-gate-pixels.json').write_text(json.dumps(result,indent=2)+'\n')
print(f'Native player gate: {result["pixels"]} source-derived visible pixels match across {len(report)} pose/countdown/normal-idle checkpoints')
