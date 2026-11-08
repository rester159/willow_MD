#!/usr/bin/env python3
"""Check actual native HUD scanout against original CPS glyphs and colors."""
from pathlib import Path
import argparse,json,zipfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--palette-probe",action="store_true")
parser.add_argument("--damage-probe",action="store_true")
args=parser.parse_args()
probe=args.palette_probe or args.damage_probe
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import palette_words,cps_rgb
from native_palette_fit import color_error
from verify_native_intro_pixels import render
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
a=json.loads(Path('build/native-hud-assets.json').read_text())
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in a['palette']]
display=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in palette]
background=render(898);patterns={};colors={};report=[]
for tick,frame in ([(n,1590) for n in range(931,951)] if probe else ((899,1588),(900,1589),(901,1590))):
 source=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 name=('player-damage-probe' if args.damage_probe else 'player-palette-probe') if probe else 'intro-probe'
 actual=Image.open(f'build/{name}-{tick}.ppm').convert('RGB')
 mismatches=[];opaque=0
 for row in (2,3,26,28,29):
  for y in range(8):
   yy=(row-2)*8+y
   for x in range(320):
    sx=(6*x+3)//5;col=sx//8;at=col*128+row*4
    code=int.from_bytes(source[at:at+2],'big');attr=int.from_bytes(source[at+2:at+4],'big');group=attr&31
    key=code,col&1
    if key not in patterns:patterns[key]=decode_tile(g,code,8,col&1)
    if group not in colors:colors[group]=[cps_rgb(w|0xf000) for w in palette_words(p,'scroll1',0,group)]
    pen=patterns[key][7-y if attr&64 else y][7-(sx&7) if attr&32 else sx&7]
    expected=background[yy*320+x]
    if pen!=15:
     index=1+min(range(15),key=lambda n:color_error(colors[group][pen],palette[n+1]));expected=display[index];opaque+=1
    if actual.getpixel((x,yy))!=expected:mismatches.append((x,yy,actual.getpixel((x,yy)),expected))
 report.append({'tick':tick,'source_frame':frame,'pixels':12800,'opaque_pixels':opaque,'mismatches':len(mismatches),'examples':mismatches[:5]})
 print(report[-1])
Path(('build/native-hud-during-player-damage.json' if args.damage_probe else 'build/native-hud-during-player-flash.json') if probe else 'build/native-hud-pixel-parity.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(not r['mismatches'] for r in report)
