#!/usr/bin/env python3
"""Late intro text/player PAL0, derived only from ROM graphics and palettes."""
from pathlib import Path
from collections import Counter
import json, zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_palette import palette_words, cps_rgb
from cps1_text_task import decode
from native_palette_fit import fit_palette, color_error
from cps1_intro_actor_render import emit

p=assemble_maincpu(Path('willow.zip'))
# The native preloader borrows the unused lower row only after this final
# single-line caption has completed. Keep that resource assumption explicit.
caption=decode(p,49,0x1305)
assert b'&' not in caption['stream'] and (caption['destination']-0x910000)%128==13*4
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
keys={(0x7020,0),(0x71a6,0)}
for message,style in [(0x419,0x306),(0x640,0x1412),(0x641,0x1413)]:
 t=decode(p,message,style);group=t['attributes'][0]&31;s=t['stream']
 if t['handler']==6:
  for i in range(0,len(s),2):
   c=int.from_bytes(s[i:i+2],'big')
   if c not in (38,47):keys.update((0x7000+c+n,group) for n in (0,1,16,17))
 else:keys.update((0x7100+c,group) for c in s if c not in (38,47))
word=lambda a:int.from_bytes(p[a:a+2],'big')
for phase in range(5):
 a=0x7c7e+word(0x7c7e+phase*2)
 for i in range(4*(1+p[0x7c78+phase])):
  keys.update((0x7000+word(a+i*2)+n,31) for n in (0,1,16,17))
colors={group:[cps_rgb(w|0xf000) for w in palette_words(p,'scroll1',0,group)] for _,group in keys}
hist=Counter();decoded={}
for code,group in sorted(keys):
 for layout in (0,1):
  pens=decode_tile(g,code,8,layout);decoded[code,group,layout]=pens
  hist.update(colors[group][pen] for row in pens for pen in row if pen!=15)
player_colors=[cps_rgb(w|0xf000) for w in palette_words(p,'base',0,0)]
player=[]
for y in range(3):
 for x in range(3):
  pens=decode_tile(g,0x33+x+y*16,16);player.append(pens)
  hist.update(player_colors[pen] for row in pens for pen in row if pen!=15)
pal=fit_palette(hist)
lookup={c:1+min(range(15),key=lambda i:color_error(c,pal[i])) for c in hist}
tiles=[bytes(32)];ids={tiles[0]:0};records=[]
for code,group in sorted(keys):
 r=[code,group]
 for layout in (0,1):
  tile=genesis_tiles([[lookup[colors[group][pen]] if pen!=15 else 0 for pen in row] for row in decoded[code,group,layout]])
  if tile not in ids:ids[tile]=len(tiles);tiles.append(tile)
  r.append(ids[tile])
 records.append(r)
# Replace only resident blocks belonging to the fixed Willow pose when PAL0
# changes. Scripted actors keep PAL1 and their original resident tile indices.
assets=json.loads(Path('build/intro-sprite-assets.json').read_text());frame=assets['frames'][-1]
pose=emit(p,{'active':1,'x':0,'y':0,'animation':frame[0]},0,0)
assert pose['width']==pose['height']==3 and pose['code']==0x33
ready_address=0xd07e+word(0xd07e+2*2)
ready=emit(p,{'active':1,'x':0,'y':0,'animation':ready_address},0,0)
assert all(ready[k]==pose[k] for k in ('x','y','code','width','height'))
assert ready['attribute']^pose['attribute']==32
player_pixels=[[15]*48 for _ in range(48)]
for ty in range(3):
 for tx in range(3):
  for y in range(16):
   for x in range(16):
    xx=tx*16+x;yy=ty*16+y
    if pose['attribute']&32:xx=47-xx
    if pose['attribute']&64:yy=47-yy
    player_pixels[yy][xx]=player[ty*3+tx][y][x]
blocks={};other={c[4] for r in assets['frames'][:-1] for c in assets['chunks'][r[3]:r[3]+r[4]]}
for x,y,w,h,offset in assets['chunks'][frame[3]:frame[3]+frame[4]]:
 assert w==h==2 and offset not in other
 pens=[row[x:x+16] for row in player_pixels[y:y+16]]
 mapped=[[lookup[player_colors[pen]] if pen!=15 else 0 for pen in row] for row in pens]
 packed=genesis_tiles(mapped)
 packed=packed[:32]+packed[64:96]+packed[32:64]+packed[96:]
 if offset in blocks:assert blocks[offset]==packed
 blocks[offset]=packed
words=[0]+[sum((v//34)<<s for v,s in zip(c,(1,5,9))) for c in pal]
direct=[0]*(max(r[0] for r in records)-0x7000+1)
for i,r in enumerate(records):
 if r[0] not in (0x7020,0x71a6):
  assert not direct[r[0]-0x7000]
  direct[r[0]-0x7000]=i+1
Path('native/intro_probe/res/overlay.bin').write_bytes(b''.join(tiles))
Path('native/intro_probe/res/overlay_player.bin').write_bytes(b''.join(blocks.values()))
rows=lambda values:','.join('{'+','.join(map(str,v))+'}' for v in values)
by_code={(r[0],r[1]):r[2:] for r in records}
commands=[];phases=[]
for phase in range(5):
 start=len(commands);pairs=1+p[0x7c78+phase];mask=0
 for side in (0,1):
  table=(0x7c64 if side else 0x7c50)+phase*4
  dest=(word(table)<<16)|word(table+2)
  source=0x7c7e+word(0x7c7e+phase*2)
  for pair in range(pairs):
   for row in range(4):
    code=0x7000+word(source);source+=2;a=dest+row*8
    for address,c in [(a,code),(a+4,code+16),(a+(-128 if side else 128),code+1),(a+(-128 if side else 128)+4,code+17)]:
     col=(address-0x910000)//128;y=((address-0x910000)%128)//4
     if 4<=col<44 and 2<=y<30:
      pattern=by_code[c,31][col&1]
      commands.append([(y-2)*43+col-4,pattern|(0x800 if side and pattern else 0)])
      mask|=1<<(y-2)
   dest+=-256 if side else 256
 phases.append([start,len(commands)-start,pairs*32,mask])
Path('native/intro_probe/src/overlay.h').write_text(
 f'#define INTRO_PLAYER_READY_ANIMATION 0x{ready_address:x}UL\n#define INTRO_PLAYER_MIRROR_WIDTH {pose["width"]*16}\n'
 f'#define INTRO_OVERLAY_ROW_STRIDE 43\n#define INTRO_OVERLAY_TILES {len(tiles)}\n#define INTRO_OVERLAY_CODES {len(records)}\n#define INTRO_OVERLAY_PLAYER_BLOCKS {len(blocks)}\n'
 'static const u16 intro_overlay_palette[16]={'+','.join(map(str,words))+'};\n'
 'static const u16 intro_overlay_codes[][4]={'+rows(records)+'};\n'
 f'#define INTRO_OVERLAY_LOOKUP_SIZE {len(direct)}\n'
 'static const u16 intro_overlay_lookup[]={'+','.join(map(str,direct))+'};\n'
 'static const u16 intro_overlay_phases[][4]={'+rows(phases)+'};\n'
 'static const u16 intro_overlay_commands[][2]={'+rows(commands)+'};\n'
 'static const u16 intro_overlay_player_offsets[]={'+','.join(map(str,blocks))+'};\n')
error=sum(n*sum((a-b)**2 for a,b in zip(c,pal[lookup[c]-1])) for c,n in hist.items())
report={'palette':words,'records':records,'player_offsets':list(blocks),'patterns':len(tiles),
        'source_colors':len(hist),'source_rgb_rmse':(error/sum(hist.values())/3)**.5,
        'phases':phases,'commands':commands,'row_stride':43}
Path('build/intro-overlay-assets.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k not in ('records','player_offsets','palette','phases','commands')})
