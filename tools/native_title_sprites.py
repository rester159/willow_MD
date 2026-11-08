#!/usr/bin/env python3
"""Extract the 154CC variant-zero stream as native 16x16 sprite pieces.
No palette slot is assigned: integration must preserve both player slots.
"""
from pathlib import Path
from collections import Counter
import json, zipfile
from PIL import Image, ImageDraw
from cps1_program import assemble_maincpu
from cps1_animation_catalog import follow_stream
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_palette import cps_rgb
from native_palette_fit import fit_palette, color_error

p=assemble_maincpu(Path('willow.zip'))
w=lambda at:int.from_bytes(p[at:at+2],'big')
signed=lambda v:v-65536 if v&32768 else v
root=0x15510+signed(w(0x15510))
stream=follow_stream(p,root)
assert stream['termination'] in ('loop','self reference')
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
frames=[];tiles={};hist=Counter()
for record in stream['records']:
 at=int(record['rom_offset'],16);count=w(at);attribute=w(at+2);shape=w(at+4)
 assert count and not count&0x8000 and not attribute&0xff00
 table_at=0x3b9b2+((shape&0x7f80)>>5)
 table=int.from_bytes(p[table_at:table_at+4],'big')
 geometry=table+signed(w(table+(shape&127)*2));pieces=[]
 for n in range(count):
  code=w(at+10+n*2);dx=signed(w(geometry+n*4));dy=signed(w(geometry+n*4+2))
  if attribute&32:dx=-dx-16
  if attribute&64:dy=-dy-16
  pens=decode_tile(g,code,16);key=(code,attribute&31)
  colors=[cps_rgb(w(0xe5080+(attribute&31)*32+i*2)|0xf000) for i in range(16)]
  pixels=[[None if pen==15 else colors[pen] for pen in row] for row in pens]
  tiles[key]=pixels
  for row in pixels:hist.update(c for c in row if c is not None)
  pieces.append([dx,dy,code,attribute])
 frames.append({'address':at,'timer':w(at+8),'pieces':pieces})
palette=fit_palette(hist)
lookup={c:1+min(range(15),key=lambda i:color_error(c,palette[i])) for c in hist}
blocks=[];ids={};tile_ids={}
for key,pixels in sorted(tiles.items()):
 raw=genesis_tiles([[0 if c is None else lookup[c] for c in row] for row in pixels])
 raw=raw[:32]+raw[64:96]+raw[32:64]+raw[96:]
 if raw not in ids:ids[raw]=len(blocks);blocks.append(raw)
 tile_ids[key]=ids[raw]
for frame in frames:
 for piece in frame['pieces']:piece.append(tile_ids[(piece[2],piece[3]&31)])
words=[0]+[sum((c//34)<<s for c,s in zip(rgb,(1,5,9))) for rgb in palette]
Path('native/intro_probe/res/title_sprites.bin').write_bytes(b''.join(blocks))
flat=[];rows=[]
for frame in frames:
 rows.append([frame['address'],frame['timer'],len(flat),len(frame['pieces'])]);flat.extend(frame['pieces'])
fmt=lambda rows:','.join('{'+','.join(map(str,row))+'}' for row in rows)
Path('native/intro_probe/src/title_sprites.h').write_text(
 '/* Original 154CC variant-zero frames; native palette slot unassigned. */\n'
 f'#define NATIVE_TITLE_BLOCKS {len(blocks)}\n#define NATIVE_TITLE_FRAMES {len(frames)}\n'
 'typedef struct {u32 address;u16 timer,first,count;} NativeTitleFrame;\n'
 'typedef struct {s16 x,y;u16 code,attribute,block;} NativeTitlePiece;\n'
 'static const NativeTitleFrame native_title_frames[]={'+fmt(rows)+'};\n'
 'static const NativeTitlePiece native_title_pieces[]={'+fmt(flat)+'};\n'
 'static const u16 native_title_palette[16]={'+','.join(map(str,words))+'};\n')
# Source vs conversion preview, fully lit. Both are derived from ROM pixels.
minx=min(q[0] for f in frames for q in f['pieces']);miny=min(q[1] for f in frames for q in f['pieces'])
maxx=max(q[0]+16 for f in frames for q in f['pieces']);maxy=max(q[1]+16 for f in frames for q in f['pieces'])
width=maxx-minx;height=maxy-miny
column=max(width+24,192)
sheet=Image.new('RGB',(column*2,(height+24)*len(frames)+24),(24,26,30));draw=ImageDraw.Draw(sheet)
draw.text((8,4),'Arcade ROM (fully lit)',fill='white')
draw.text((column+8,4),'Genesis conversion',fill='white')
for fi,frame in enumerate(frames):
 for converted in (0,1):
  origin=(8+converted*column,24+fi*(height+24))
  for dx,dy,code,attribute,_ in frame['pieces']:
   pixels=tiles[(code,attribute&31)]
   for y,row in enumerate(pixels):
    for x,c in enumerate(row):
     if c is None:continue
     xx=15-x if attribute&32 else x;yy=15-y if attribute&64 else y
     sheet.putpixel((origin[0]+dx-minx+xx,origin[1]+dy-miny+yy),palette[lookup[c]-1] if converted else c)
  draw.text((origin[0],origin[1]+height+2),f'{frame["address"]:05X}',fill='white')
sheet.save('build/native-title-sprites.png')
report={'source_root':hex(root),'termination':stream['termination'],'frames':frames,'palette':words,
 'unique_blocks':len(blocks),'bytes':len(blocks)*128,'source_colors':len(hist),'palette_slot':None,
 'scope':'Unlinked sprite assets; fully-lit ROM comparison, not emulator output'}
Path('build/native-title-sprites.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k not in ('frames','palette')})
