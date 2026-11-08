#!/usr/bin/env python3
"""Build Willow gameplay sprites from source-selected animation streams."""
from pathlib import Path
import json,zipfile
from cps1_program import assemble_maincpu
from cps1_animation_catalog import follow_stream
from cps1_intro_actor_render import emit
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb
from native_palette_fit import color_error
p=assemble_maincpu(Path('willow.zip'));word=lambda a:int.from_bytes(p[a:a+2],'big')
# Selector domains established by the translated player handlers. Death masks
# facing to one bit; movement/jump use the full four facing/stationary values.
roots={0xd076:4,0xd07e:4,0xd086:4,0xd08e:2,0xd1a2:2,0xd1a6:2,0xd1aa:2,0xd1ae:2,0xd1b2:2,0xd1b6:2,0xd27a:4,0xd2ca:2,0xd2ce:2,0xd33a:2,0xd3be:2,0xd3c2:2,0xd436:2}
poses={}
for table,count in roots.items():
 for facing in range(count):
  stream=follow_stream(p,table+word(table+facing*2))
  assert stream['termination'] in ('loop','self reference','long-delay/control record')
  for r in stream['records']:
   at=int(r['rom_offset'],16);assert 0xd092<=at<0xd4ba
   # A FFFF hold in death is removed by the 40-update controller first.
   assert r['timer'] and (r['timer']<=0x1000 or r['timer']==65535)
   poses[at]=emit(p,{'active':1,'x':128,'y':128,'animation':at},0,0)
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
source={};frames=[];chunks=[]
for address,o in sorted(poses.items()):
 assert o['attribute']&31==0
 first=len(chunks)
 for ty in range(o['height']):
  for tx in range(o['width']):
   code=(o['code']&~15)+((o['code']+tx)&15)+ty*16
   pens=decode_tile(g,code,16)
   if all(v==15 for row in pens for v in row):continue
   source[code]=pens
   x=(o['width']-1-tx if o['attribute']&32 else tx)*16
   y=(o['height']-1-ty if o['attribute']&64 else ty)*16
   chunks.append([x,y,code])
 frames.append([address,first,len(chunks)-first,o['x']-192,o['y']-128,o['attribute']&96])
assert max(f[2] for f in frames)<=9
codes=sorted(source);code_ids={code:i for i,code in enumerate(codes)}
for c in chunks:c[2]=code_ids[c[2]]
hud=json.loads(Path('build/native-hud-assets.json').read_text())
palette=[tuple(((w>>s)&7)*34 for s in (1,5,9)) for w in hud['palette'][1:]]
blocks=[bytes(128)];ids={blocks[0]:0};variants=[]
for at in (0xe5080,0xe5000,0xe5040):
 for brightness in range(16):
  mapping=[1+min(range(15),key=lambda n:color_error(cps_rgb(word(at+pen*2)|(brightness<<12)),palette[n])) for pen in range(16)];mapping[15]=0
  row=[]
  for code in codes:
   raw=genesis_tiles([[mapping[v] for v in r] for r in source[code]])
   raw=raw[:32]+raw[64:96]+raw[32:64]+raw[96:]
   if raw not in ids:ids[raw]=len(blocks);blocks.append(raw)
   row.append(ids[raw])
  variants.append(row)
assert len(blocks)<65536
start=next(f for f in frames if f[0]==0xd15a);assert start[2]==8
initial=[variants[15][c[2]] for c in chunks[start[1]:start[1]+start[2]]]
Path('native/intro_probe/res/player_sprites.bin').write_bytes(b''.join(blocks))
Path('native/intro_probe/res/player_start.bin').write_bytes(b''.join(blocks[i] for i in initial))
rows=lambda values:','.join('{'+','.join(map(str,row))+'}' for row in values)
Path('native/intro_probe/src/player_sprites.h').write_text('/* ROM-selected Willow poses; no trace or video inputs. */\n'
 f'#define NATIVE_PLAYER_FRAMES {len(frames)}\n#define NATIVE_PLAYER_SOURCE_TILES {len(codes)}\n'
 'typedef struct {u32 address;u16 first,count;s16 x,y;u16 flip;} NativePlayerFrame;\n'
 'typedef struct {s16 x,y;u16 source;} NativePlayerChunk;\n'
 'static const NativePlayerFrame native_player_frames[]={'+rows(frames)+'};\n'
 'static const NativePlayerChunk native_player_chunks[]={'+rows(chunks)+'};\n'
 f'static const u16 native_player_variants[48][{len(codes)}]={{'+rows(variants)+'};\n'
 'static const u16 native_player_start_ids[]={'+','.join(map(str,initial))+'};\n')
report={'roots':roots,'frames':frames,'chunks':chunks,'source_codes':codes,'variants':variants,'blocks':len(blocks),'rom_bytes':len(blocks)*128,
 'scope':'All 70 blocked-record poses selected by the translated Willow handlers, including FFFF holds; full campaign/player selection and renderer integration verified separately.'}
Path('build/native-player-sprites.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k not in ('roots','frames','chunks','source_codes','variants')})
