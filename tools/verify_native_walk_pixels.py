#!/usr/bin/env python3
"""Independent ROM-coordinate walk compositor; does not read native SAT/maps."""
from pathlib import Path
from functools import lru_cache
import json
from PIL import Image
import verify_native_background_pairs as bg
from verify_native_intro_pixels import sprite_assets, sprite_bytes, sprite_palette, actor_trace
from cps1_intro_actor_render import emit
from cps1_gfx import decode_tile
from cps1_palette import palette_words, cps_rgb
from native_palette_fit import color_error

walk=Path('build/arcade-trace/player-walk/frames.bin').read_bytes()
far=Path('build/arcade-trace/scroll3-walk/frames.bin').read_bytes()
def record(t):
 at=(t-897)*40198
 assert int.from_bytes(walk[at:at+4],'big')==t+690
 return walk[at:at+40198]
def camera(t,layer):
 if layer==2:return int.from_bytes(record(t)[1036:1038],'big')
 at=(t+690-650)*1286
 assert int.from_bytes(far[at:at+4],'big')==t+690
 return int.from_bytes(far[at+4+0x488:at+4+0x48a],'big')
def player(t):
 r=record(t)[1158:1286]
 return dict(active=r[0],x=int.from_bytes(r[6:8],'big'),y=int.from_bytes(r[10:12],'big'),animation=int.from_bytes(r[20:24],'big'))
lookup={}
for layer in (2,3):
 pairs=bg.array((bg.native/f'src/scroll{layer}_data.h').read_text(),f'nativeScroll{layer}Pairs')
 lookup[layer]={((0x4000+((pair>>16)&8191)) if layer==2 else pair>>16,pair&65535):i for i,pair in enumerate(pairs)}
display=[[(r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31] for bank in bg.palettes for r,g,b in bank]
@lru_cache(None)
def tile(layer,col,row):
 x,y=col*8+32,row*8-240
 code,attr=bg.source.record(layer,x,y)
 pair=lookup[layer][code&65535,attr]
 offsets,entries=bg.entries_by_layer[layer];size=16 if layer==2 else 32
 entry=entries[offsets[pair]+(y%size//8)*(size//8)+x%size//8]
 pattern=entry&32767;palette=(entry>>15)*16
 data=bg.blob[pattern*32:(pattern+1)*32]
 return [tuple(display[palette+((data[i//2]>>(0 if i&1 else 4))&15)]) if ((data[i//2]>>(0 if i&1 else 4))&15) else None for i in range(64)]
@lru_cache(None)
def background(near_x,far_x):
 out=[]
 for y in range(224):
  for x in range(320):
   color=None
   for layer,cx in ((2,near_x),(3,far_x)):
    xx=x+cx;color=tile(layer,xx//8,y//8)[(y%8)*8+xx%8]
    if color is not None:break
   out.append(color or (0,0,0))
 return out
frames={r[0]:r for r in sprite_assets['frames']}
def npc(out,slot,cx):
 a=actor_trace[1587,slot];f=frames[a['animation']]
 for ox,oy,w,h,offset in sprite_assets['chunks'][f[3]:f[3]+f[4]]:
  for y in range(h*8):
   for x in range(w*8):
    at=(offset+(x//8)*h+y//8)*32+(y%8)*4+(x%8)//2
    pen=(sprite_bytes[at]>>(0 if x&1 else 4))&15
    xx=a['x']-cx+f[5]-32+ox+x;yy=256-a['y']+f[6]-16+oy+y
    if pen and 0<=xx<320 and 0<=yy<224:out[yy*320+xx]=sprite_palette[pen]
hud=json.loads(Path('build/native-hud-assets.json').read_text())
pal=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in hud['palette'][1:]]
player_display=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in pal]
colors=[cps_rgb(w|0xf000) for w in palette_words(bg.program,'base',0,0)]
mapping=[player_display[min(range(15),key=lambda n:color_error(color,pal[n]))] for color in colors]
@lru_cache(None)
def graphic(code):return decode_tile(bg.gfx,code,16)
def draw_player(out,t,cx):
 a=player(t);a['animation']=player(t-1)['animation'];o=emit(bg.program,a,cx,0)
 if not o:return
 for ty in range(o['height']):
  for tx in range(o['width']):
   data=graphic((o['code']&~15)+((o['code']+tx)&15)+ty*16)
   for y in range(16):
    for x in range(16):
     pen=data[y][x]
     if pen==15:continue
     xx=tx*16+x;yy=ty*16+y
     if o['attribute']&32:xx=o['width']*16-1-xx
     if o['attribute']&64:yy=o['height']*16-1-yy
     xx+=o['x']-96;yy+=o['y']-16
     if 0<=xx<320 and 0<=yy<224:out[yy*320+xx]=mapping[pen]

def main():
 reports=[]
 for tick in (936,940,950,960,970,976,980,990,1000,1010):
  out=background(camera(tick-2,2),camera(tick-2,3)).copy()
  cx=camera(tick-2,2)
  npc(out,0,cx);draw_player(out,tick-1,cx);npc(out,1,cx)
  actual=list(Image.open(f'build/player-walk-native-{tick}.ppm').convert('RGB').getdata())
  indices=[y*320+x for y in range(16,192) for x in range(320)]
  bad=[i for i in indices if actual[i]!=out[i]]
  result=dict(tick=tick,pixels=len(indices),mismatches=len(bad),examples=[(i%320,i//320,actual[i],out[i]) for i in bad[:3]])
  if tick==950:
   wrong=background(camera(tick-1,2),camera(tick-1,3))
   result['wrong_camera_phase_mismatches']=sum(actual[y*320+x]!=wrong[y*320+x] for y in range(16,48) for x in range(320))
   wrong=background(camera(tick-2,2),camera(tick-2,3)).copy()
   npc(wrong,0,cx);draw_player(wrong,tick-2,cx);npc(wrong,1,cx)
   result['wrong_player_phase_mismatches']=sum(actual[i]!=wrong[i] for i in indices)
   assert result['wrong_camera_phase_mismatches'] and result['wrong_player_phase_mismatches'], 'Phase controls must distinguish adjacent frames'
  reports.append(result);print(result,flush=True)
 Path('build/native-walk-pixel-parity.json').write_text(json.dumps(dict(frames=reports,scope='ROM-coordinate backgrounds and original player sprite geometry; frozen scripted actors from the verified intro handoff. Excludes HUD rows.'),indent=2)+'\n')
 assert not any(r['mismatches'] for r in reports)
if __name__=='__main__':main()
