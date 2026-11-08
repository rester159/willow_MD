#!/usr/bin/env python3
"""Bake source layer geometry for the isolated native intro renderer probe.
No captured pixels or per-frame playback tables are used. Camera logic remains C.
"""
from pathlib import Path
import zipfile,json
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,genesis_tiles
from cps1_palette import palette_words,cps_rgb,genesis_color
from cps1_source_layers import SourceLayers
from native_palette_fit import fit_tile_palettes
ROOT=Path(__file__).resolve().parents[1]
p=assemble_maincpu(ROOT/'willow.zip')
with zipfile.ZipFile(ROOT/'willow.zip') as z:g=assemble_region(z)
layers=SourceLayers(p,g);cells=[];opaque=[];offsets=[]
for layer,origin,height in [(2,-752,736),(3,-496,480)]:
 offsets.append(len(cells))
 pals=[[cps_rgb(w|0xf000) for w in palette_words(p,f'scroll{layer}',0,gr)] for gr in range(32)]
 for ty in range(height//8):
  for tx in range(40):
   cell=[];mask=[]
   for py in range(8):
    for px in range(8):
     pen,group=layers.pen(layer,32+tx*8+px,origin+ty*8+py)
     cell.append(pals[group][pen] if pen!=15 else (0,0,0));mask.append(pen!=15)
   cells.append(cell);opaque.append(mask)
palettes,slots,lookup=fit_tile_palettes(cells)
tiles=[bytes(32)];ids={tiles[0]:0};entries=[]
for pixels,mask in zip(cells,opaque):
 slot=min(range(2),key=lambda s:sum(lookup[c][s][0] for c,on in zip(pixels,mask) if on))
 pens=[lookup[c][slot][1] if on else 0 for c,on in zip(pixels,mask)]
 tile=genesis_tiles([pens[y*8:y*8+8] for y in range(8)])
 if tile not in ids:ids[tile]=len(tiles);tiles.append(tile)
 entries.append(ids[tile]|(slot<<15))
maximum=0
for cy in range(513):
 visible=set()
 for base,y,back in [(offsets[0],512-cy,False),(offsets[1],256-(cy>>1),True)]:
  start=y//8;end=(y+226)//8 if back else start+28
  for row in range(start,end+1):
   for x in range(40):
    at=base+row*40+x
    if at<len(entries):visible.add(entries[at]&32767)
 maximum=max(maximum,len(visible))
assert maximum<=1400,maximum
out=ROOT/'native/intro_probe/res';(out/'layers.bin').write_bytes(b''.join(tiles))
lines=['/* Generated from source ROM layer tables. */',f'#define INTRO_TILE_COUNT {len(tiles)}',f'#define INTRO_BG_OFFSET {offsets[1]}',f'#define INTRO_MAX_VISIBLE {maximum}']
lines.append('static const u16 intro_palettes[2][16]={'+','.join('{0,'+','.join(hex(((c[0]//34)<<1)|((c[1]//34)<<5)|((c[2]//34)<<9)) for c in pal)+'}' for pal in palettes)+'};')
lines.append('static const u16 intro_maps[]={'+','.join(hex(n) for n in entries)+'};')
(ROOT/'native/intro_probe/src/layers.h').write_text('\n'.join(lines)+'\n')
report={'tiles':len(tiles),'bytes':len(tiles)*32,'max_visible_patterns':maximum,'layers':'independent source Scroll2 and Scroll3','crop':'32 pixels from each side of 384-wide source; no resampling','scope':'renderer probe; no source message/actor/fade tasks'}
(ROOT/'build/native-intro-layers.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
