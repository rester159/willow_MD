#!/usr/bin/env python3
"""Source glyph pixels vs exact declared projection to unchanged carrier CRAM.
This is a resource/opacity proof, not native VDP or artistic fidelity admission.
"""
from pathlib import Path
from collections import Counter
import json,struct,zipfile,hashlib,math
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb
from native_palette_fit import color_error
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/native-opening-hud-audit/v1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 evidence=json.loads((OUT/'evidence.json').read_text());keys=json.loads((OUT/'source-keys.json').read_text());assert all((k['source_attribute']&0x60)==0 for k in keys);cramp=ROOT/'build/native-route-objects/prepared-v2/cram.bin';cram=struct.unpack('>64H',cramp.read_bytes());pages=[[tuple(((w>>s)&7)*34 for s in(1,5,9))for w in cram[i*16:(i+1)*16]]for i in range(4)]
 with zipfile.ZipFile(ROOT/'willow.zip')as z:gfx=assemble_region(z)
 observed={bytes(32):0};variants={bytes(32):0};maps=[];checks=0;opaque=0;error=0;sourcewords={int(k):v for k,v in evidence['source_palette_words'].items()}
 for key in keys:
  code=key['code'];attr=key['source_attribute'];layout=key['layout'];group=attr&31;pixels=decode_tile(gfx,code,8,layout);colors=[cps_rgb(w)for w in sourcewords[group]];hist=Counter(colors[v]for row in pixels for v in row if v!=15)
  pagecost=[]
  for page in range(4):
   pens=[min(range(1,16),key=lambda k:(color_error(color,pages[page][k]),k))for color in colors]
   pagecost.append((sum(n*min(color_error(c,pages[page][k])for k in range(1,16))for c,n in hist.items()),page,pens))
  _,page,pens=min(pagecost,key=lambda p:(p[0],p[1]));variant_ids=[];base=genesis_tiles([[0 if v==15 else pens[v]for v in row]for row in pixels])
  for flip in range(4):
   matrix=[[0 if(v:=pixels[7-y if flip&2 else y][7-x if flip&1 else x])==15 else pens[v]for x in range(8)]for y in range(8)];raw=genesis_tiles(matrix)
   if raw not in variants:variants[raw]=len(variants)
   variant_ids.append(variants[raw])
   if flip==((attr>>5)&3):
    if raw not in observed:observed[raw]=len(observed)
   for y in range(8):
    for x in range(8):
     v=pixels[7-y if flip&2 else y][7-x if flip&1 else x];n=(raw[y*4+x//2]>>(0 if x&1 else 4))&15
     assert(n==0)==(v==15);assert n==(0 if v==15 else pens[v]);bx=7-x if flip&1 else x;by=7-y if flip&2 else y;hardware=(base[by*4+bx//2]>>(0 if bx&1 else 4))&15;assert hardware==n;checks+=1
     if flip==((attr>>5)&3)and v!=15:opaque+=1;error+=sum((a-b)**2 for a,b in zip(colors[v],pages[page][n]))
  maps.append({'code':code,'full_attribute':attr,'layout':layout,'group':group,'page':page,'source_words':sourcewords[group],'source_pen_to_target_pen':pens,'all_flip_pattern_ids':variant_ids})
 blob=b''.join(variants);(OUT/'projection-all-flips.bin').write_bytes(blob);(OUT/'projection-maps.json').write_text(json.dumps(maps,indent=2)+'\n')
 result={'scope':__doc__,'keys':len(keys),'verified_original_geometry_pixels':checks,'observed_nonblank_patterns':len(observed)-1,'all_flip_nonblank_patterns':len(variants)-1,'all_flip_pattern_bytes':len(blob),'all_flips_fit105_nonblank_slots':len(variants)-1<=105,'native_single_cell_flip_alternative':{'verified_pixels':checks,'base_patterns':len(observed)-1,'resident_bytes':(len(observed)-1)*32,'condition':'All currently observed source attributes have flip0; storing these base patterns and applying Genesis individual8x8 H/V flip bits exactly reproduces all four source flip variants. Existing pre-oriented Scroll1SatRow must not coalesce flipped entries.'},'cram_words_unchanged':64,'both_player_pages_unchanged':True,'source_transparency_exact':'Every CPS pen15 becomes nativepen0; every other CPS pen remains nonzero, including source black.','source_colors':'Full cps_rgb(word), including brightness, used before fitting; no RGB truncation before fit.','observed_unique_key_opaque_pixel_rgb_rmse':math.sqrt(error/(3*opaque)),'rmse_scope':'Each observed code/fullattr/layout key weighted once, not screen-frequency visual quality.','artifact_scope':'Read-only candidate projection, no renderer integration or combined actual-VDP pixel proof.','inputs':{str(p):sha(p)for p in[Path(__file__),OUT/'evidence.json',OUT/'source-keys.json',cramp,ROOT/'willow.zip']},'products':{str(p.name):sha(p)for p in[OUT/'projection-all-flips.bin',OUT/'projection-maps.json']}}
 (OUT/'palette-pixel-proof.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k not in('inputs','products')},indent=2))
if __name__=='__main__':main()
