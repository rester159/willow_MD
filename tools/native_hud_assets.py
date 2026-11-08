#!/usr/bin/env python3
"""ROM-derived gameplay HUD patterns and resident Willow palette variant."""
from pathlib import Path
from collections import Counter
import csv,json,zipfile
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import palette_words,cps_rgb
from native_palette_fit import fit_palette,color_error
from cps1_intro_actor_render import emit
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
keys=[(int(r['code']),int(r['palette'])) for r in csv.DictReader(open('build/arcade-hud-tile-keys.csv'))]
colors={group:[cps_rgb(w|0xf000) for w in palette_words(p,'scroll1',0,group)] for _,group in keys}
raw={};hist=Counter()
for code,group in keys:
 for layout in (0,1):
  pens=decode_tile(g,code,8,layout);raw[code,group,layout]=pens
  hist.update(colors[group][pen] for row in pens for pen in row if pen!=15)
# The current idle Willow shares PAL0. Include its original pixels in fitting;
# this does not establish a palette for the full set of gameplay sprites.
player_colors=[cps_rgb(w|0xf000) for w in palette_words(p,'base',0,0)]
for y in range(3):
 for x in range(3):
  pens=decode_tile(g,0x33+x+y*16,16)
  hist.update(player_colors[pen] for row in pens for pen in row if pen!=15)
pal=fit_palette(hist)
lookup={c:1+min(range(15),key=lambda n:color_error(c,pal[n])) for c in hist}
words=[0]+[sum((v//34)<<s for v,s in zip(rgb,(1,5,9))) for rgb in pal]
tiles=[bytes(32)];ids={tiles[0]:0};records=[]
for code,group in keys:
 record=[code,group]
 for layout in (0,1):
  pens=raw[code,group,layout]
  data=genesis_tiles([[0 if v==15 else lookup[colors[group][v]] for v in row] for row in pens])
  if data not in ids:ids[data]=len(tiles);tiles.append(data)
  record.append(ids[data])
 records.append(record)
# Precompute the two disjoint source contributions for every 320px output
# tile. Runtime composition needs eight longword ORs, not per-pixel scaling.
xmaps=[[(6*(phase*8+x)+3)//5-phase*8 for x in range(8)] for phase in range(5)]
projections=[bytes(32)];projection_ids={projections[0]:0};projection_rows=[]
for tile in tiles:
 row=[]
 for flip in (False,True):
  for phase in range(5):
   for side in range(2):
    data=bytearray(32)
    for y in range(8):
     for x,sx in enumerate(xmaps[phase]):
      if sx//8!=side:continue
      px=7-(sx&7) if flip else sx&7
      v=(tile[y*4+px//2]>>(0 if px&1 else 4))&15
      data[y*4+x//2]|=v<<(0 if x&1 else 4)
    data=bytes(data)
    if data not in projection_ids:projection_ids[data]=len(projections);projections.append(data)
    row.append(projection_ids[data])
 projection_rows.append(row)
Path('native/intro_probe/res/hud_projected.bin').write_bytes(b''.join(projections))
Path('native/intro_probe/res/hud.bin').write_bytes(b''.join(tiles))
# PAL0 is shared with the resident Willow pose. Repack that same ROM pose
# when the HUD palette takes ownership; never reinterpret old palette indices.
actors=json.loads(Path('build/intro-sprite-assets.json').read_text())
frame=actors['frames'][-1]
pose=emit(p,{'active':1,'x':0,'y':0,'animation':frame[0]},0,0)
assert pose['width']==pose['height']==3 and pose['code']==0x33
player_pixels=[[0]*48 for _ in range(48)]
for ty in range(3):
 for tx in range(3):
  pens=decode_tile(g,0x33+tx+ty*16,16)
  for y in range(16):
   for x in range(16):
    xx=tx*16+x;yy=ty*16+y
    if pose['attribute']&32:xx=47-xx
    if pose['attribute']&64:yy=47-yy
    pen=pens[y][x]
    player_pixels[yy][xx]=0 if pen==15 else lookup[player_colors[pen]]
player_blocks={}
for x,y,w,h,offset in actors['chunks'][frame[3]:frame[3]+frame[4]]:
 assert w==h==2
 packed=genesis_tiles([row[x:x+16] for row in player_pixels[y:y+16]])
 packed=packed[:32]+packed[64:96]+packed[32:64]+packed[96:]
 if offset in player_blocks:assert player_blocks[offset]==packed
 player_blocks[offset]=packed
old_overlay=json.loads(Path('build/intro-overlay-assets.json').read_text())
assert list(player_blocks)==old_overlay['player_offsets']
Path('native/intro_probe/res/hud_player.bin').write_bytes(b''.join(player_blocks.values()))

codes=sorted({r[0] for r in records});code_min=min(codes);code_max=max(codes)
code_rows={code:n for n,code in enumerate(codes)}
directory=[65535]*(code_max-code_min+1)
palette_rows=[[0]*32 for _ in codes]
for code,n in code_rows.items():directory[code-code_min]=n
for n,r in enumerate(records):palette_rows[code_rows[r[0]]][r[1]]=n+1
Path('native/intro_probe/src/hud.h').write_text('/* Generated from ROM-derived source HUD writes. */\n'
 f'#define NATIVE_HUD_TILES {len(tiles)}\n#define NATIVE_HUD_CODES {len(records)}\n#define NATIVE_HUD_PROJECTIONS {len(projections)}\n'
 f'#define NATIVE_HUD_CODE_MIN {code_min}\n#define NATIVE_HUD_CODE_MAX {code_max}\n'
 'static const unsigned short native_hud_directory[]={'+','.join(map(str,directory))+'};\n'
 'static const unsigned short native_hud_palette_rows[][32]={'+','.join('{'+','.join(map(str,r))+'}' for r in palette_rows)+'};\n'
 'static const unsigned short native_hud_palette[16]={'+','.join(map(str,words))+'};\n'
 'static const unsigned short native_hud_codes[][4]={'+','.join('{'+','.join(map(str,r))+'}' for r in records)+'};\n'
 'static const unsigned short native_hud_projection_ids[][20]={'+','.join('{'+','.join(map(str,r))+'}' for r in projection_rows)+'};\n')
error=sum(n*sum((a-b)**2 for a,b in zip(c,pal[lookup[c]-1])) for c,n in hist.items())
report={'status':'ROM-derived HUD resource pack; native integration verification reported separately','records':records,'patterns':len(tiles),
 'lookup_rom_bytes':len(directory)*2+len(palette_rows)*64,'projected_patterns':len(projections),'projected_rom_bytes':len(projections)*32+len(projection_rows)*40,'projection_ids':projection_rows,
 'player_offsets':list(player_blocks),'player_pattern_bytes':len(player_blocks)*128,
 'palette':words,'source_colors':len(hist),'source_rgb_rmse':(error/sum(hist.values())/3)**.5,
 'scope':'All enumerated source HUD writes, with the current idle Willow pose in palette fitting; arbitrary moving-player palettes remain pending.'}
Path('build/native-hud-assets.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k not in ('records','palette','projection_ids')})
