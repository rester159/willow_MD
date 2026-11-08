#!/usr/bin/env python3
"""Build/verify a four-page background fixture from original intro layer pixels.

Player CRAM pages are immutable plan inputs, reusable by background tiles.
Map words encode two page bits and a 14-bit ROM pattern ID, NOT a VDP address.
The runtime still owns actual VRAM residency, source-palette admission and the
shared object/background palette epoch. This tool changes no production files.
"""
from pathlib import Path
from collections import Counter
import argparse,hashlib,json,math,re,struct,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,genesis_tiles
from cps1_palette import cps_rgb,palette_words
from cps1_source_layers import SourceLayers
from native_palette_fit import color_error
from audit_native_object_palette_fit import cram_rgb
from fit_native_object_palette_anchors import is_neutral_stone,preserves_neutral

ROOT=Path(__file__).resolve().parents[1]
def build(plan_path,output):
 plan=json.loads(plan_path.read_text());pals=[[cram_rgb(w) for w in p[1:]] for p in plan['cram']];allowed=plan['background_pages']
 assert set(allowed)<=set(range(4)) and len(pals)==4
 program=assemble_maincpu(ROOT/'willow.zip')
 with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
 layers=SourceLayers(program,gfx);source_words={layer:[[w|0xf000 for w in palette_words(program,f'scroll{layer}',0,g)] for g in range(32)] for layer in (2,3)}
 patterns=[bytes(32)];ids={patterns[0]:0};entries=[];cells=[];lookup={};opaque_tiles=Counter();all_tiles=Counter();visible_pixels=Counter();source_colors=set();opaque_error=0;neutral_checks=0;total_pixels=0
 for layer,origin,height in ((2,-752,736),(3,-496,480)):
  for ty in range(height//8):
   for tx in range(40):
    pixels=[]
    for y in range(8):
     for x in range(8):
      pen,group=layers.pen(layer,32+tx*8+x,origin+ty*8+y)
      pixels.append(cps_rgb(source_words[layer][group][pen]) if pen!=15 else None)
    hist=Counter(c for c in pixels if c is not None)
    for color in hist:
     if color not in lookup:
      lookup[color]=[min(((color_error(color,p),j) for j,p in enumerate(page) if not plan.get('neutral_stone_constraint') or not is_neutral_stone(color) or preserves_neutral(color,p)),default=None) for page in pals]
    choices=[page for page in allowed if all(lookup[c][page] is not None for c in hist)]
    assert choices,'No representable page under the declared neutral constraint'
    page=min(choices,key=lambda page:(sum(n*lookup[c][page][0] for c,n in hist.items()),page))
    pens=[0 if c is None else 1+lookup[c][page][1] for c in pixels]
    tile=genesis_tiles([pens[y*8:y*8+8] for y in range(8)]);assert len(tile)==32
    if tile not in ids:ids[tile]=len(patterns);patterns.append(tile)
    pattern=ids[tile];assert pattern<16384
    entries.append((page<<14)|pattern);cells.append((pixels,page,pattern))
    all_tiles[page]+=1
    if hist:opaque_tiles[page]+=1
    for color,n in hist.items():
     output_color=pals[page][lookup[color][page][1]];opaque_error+=n*sum((a-b)**2 for a,b in zip(color,output_color));source_colors.add(color);visible_pixels[page]+=n;total_pixels+=n
 blob=b''.join(patterns);maps=struct.pack('>'+str(len(entries))+'H',*entries)
 # Decode the stored map fields and 4bpp bytes independently, then compare
 # every transparent/opaque pixel and chosen source-color reconstruction.
 checks=0;rebuilt_source=set();used=[set() for _ in range(4)]
 for index,(pixels,expected_page,expected_pattern) in enumerate(cells):
  value=int.from_bytes(maps[index*2:index*2+2],'big');page=value>>14;pattern=value&0x3fff
  assert page==expected_page and pattern==expected_pattern
  for y in range(8):
   for x in range(8):
    color=pixels[y*8+x];byte=blob[pattern*32+y*4+x//2];pen=(byte>>(0 if x&1 else 4))&15
    assert (pen==0)==(color is None)
    if color is not None:
     assert pen==1+lookup[color][page][1];out=pals[page][pen-1];used[page].add(out);rebuilt_source.add(color)
     if plan.get('neutral_stone_constraint') and is_neutral_stone(color):assert preserves_neutral(color,out);neutral_checks+=1
    checks+=1
 output.parent.mkdir(parents=True,exist_ok=True);output.with_suffix('.bin').write_bytes(blob);output.with_suffix('.map16be').write_bytes(maps)
 name=re.sub('[^a-zA-Z0-9_]','_',output.name)
 output.with_suffix('.h').write_text('\n'.join([
  '/* Offline four-page background fixture; not admitted to production. */',
  f'#ifndef {name.upper()}_H',f'#define {name.upper()}_H','#include <stdint.h>',
  f'#define {name.upper()}_PATTERNS {len(patterns)}',f'#define {name.upper()}_CELLS {len(entries)}',
  f'extern const uint8_t {name}_patterns[{len(blob)}];',
  f'extern const uint16_t {name}_map[{len(entries)}];',
  '/* ROM map page = value >> 14; pattern ID = value & 0x3fff.',
  ' * VDP attribute page is <<13 after resolving the physical cache tile.',
  ' * Source/CRAM palette epoch must match object and text presentation. */','#endif'])+'\n')
 output.with_suffix('.s').write_text(f'.section .rodata\n.balign 4\n.global {name}_patterns\n{name}_patterns:\n.incbin "{output.with_suffix(".bin").resolve()}"\n.balign 2\n.global {name}_map\n{name}_map:\n.incbin "{output.with_suffix(".map16be").resolve()}"\n')
 report={'name':plan['name'],'pattern_count':len(patterns),'pattern_bytes':len(blob),'map_cells':len(entries),'map_bytes':len(maps),'map_format':{'bits15_14':'Genesis CRAM page 0..3','bits13_0':'Immutable ROM 8x8 pattern ID, not physical VRAM tile','runtime_attribute':'(decoded_page << 13) | (cache_physical_tile & 0x7ff), with source-derived flip/priority supplied separately if required','mutability':'Every referenced player/background CRAM change invalidates this plan and its background pen interpretation. Use the same presentation epoch as object remaps; rebuild/refit or select a preverified plan, never repaint cached tiles with stale page bindings.'},'source_layers':[{'layer':2,'x':32,'y':-752,'width':320,'height':736,'first_cell':0},{'layer':3,'x':32,'y':-496,'width':320,'height':480,'first_cell':3680}],'source_palette_words':source_words,'cram':plan['cram'],'background_pages':allowed,'all_cell_page_counts':all_tiles,'opaque_cell_page_counts':opaque_tiles,'opaque_pixel_page_counts':visible_pixels,'used_rgb_by_page':{page:sorted(colors) for page,colors in enumerate(used)},'source_visible_colors':len(source_colors),'source_pixels_verified':checks,'source_neutral_pixels_verified':neutral_checks,'opaque_pixels':total_pixels,'rgb_rmse':math.sqrt(opaque_error/(total_pixels*3)),'patterns_sha256':hashlib.sha256(blob).hexdigest(),'map_sha256':hashlib.sha256(maps).hexdigest(),'scope':'ROM-decoded opening layer geometry with explicit four-page pen fit. Immutable player page values reused, never overwritten. Offline integration fixture only; no actual cartridge VRAM, layer priority, background source-palette epoch or DMA admission.'}
 output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');return {k:v for k,v in report.items() if k not in ('source_palette_words','used_rgb_by_page','cram')}
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--plan',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();print(json.dumps(build(args.plan,args.output),indent=2))
