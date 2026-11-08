#!/usr/bin/env python3
"""Source-code keyed Scroll1/font/overlay/HUD remaps into an immutable CRAM plan.

No palette writes or resampling. Both CPS 8x8 layouts and all source flips are
explicit original-code variants. This permits contiguous row staging/SAT runs
without conflating whole-sprite and individual-glyph horizontal flips.
"""
from pathlib import Path
from collections import Counter
import argparse,csv,hashlib,json,math,struct,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb,palette_words
from native_palette_fit import color_error
from audit_native_object_palette_fit import cram_rgb

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit'
def source_cell(data,col,row,base=0):
 at=base+(row&31)*4+col*128+((row&32)<<8)
 return struct.unpack('>HH',data[at:at+4])
def collect_keys():
 keys={(code,3) for code in range(0x7020,0x7060)}
 keys.update((r[0],r[1]) for r in json.loads((ROOT/'build/intro-overlay-assets.json').read_text())['records'])
 keys.update((int(r['code']),int(r['palette'])) for r in csv.DictReader((ROOT/'build/arcade-hud-tile-keys.csv').open()))
 captures=[];excluded=Counter()
 for path in sorted((ROOT/'build/arcade-trace').glob('intro-vram-*.bin')):
  raw=path.read_bytes();captures.append({'path':str(path),'base':0x10000,'frame':int(path.stem.split('-')[-1]),'kind':'intro_vram'})
  for row in range(2,30):
   for col in range(4,44):
    code,attr=source_cell(raw,col,row,0x10000)
    if 0x7000<=code<=0x7fff:keys.add((code,attr&31))
    else:excluded[code]+=1
 for path in sorted((ROOT/'build/arcade-trace/actor-native').glob('text-*.bin')):
  raw=path.read_bytes();captures.append({'path':str(path),'base':0,'frame':int(path.stem.split('-')[-1]),'kind':'text_ram'})
  # Include complete 384px HUD/overlay width, keeping width adaptation a
  # separate explicit renderer concern. No compressed glyphs are generated.
  for row in range(2,31):
   for col in range(48):
    code,attr=source_cell(raw,col,row)
    if 0x7000<=code<=0x7fff:keys.add((code,attr&31))
    else:excluded[code]+=1
 return sorted(keys),captures,excluded
def build(plan_path,output):
 plan=json.loads(plan_path.read_text());pages=[[cram_rgb(w) for w in p[1:]] for p in plan['cram']]
 program=assemble_maincpu(ROOT/'willow.zip')
 with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
 keys,captures,excluded=collect_keys();groups=sorted({group for _,group in keys})
 words={group:[w|0xf000 for w in palette_words(program,'scroll1',0,group)] for group in groups}
 colors={group:[cps_rgb(w) for w in words[group]] for group in groups};decoded={};patterns=[bytes(32)];ids={patterns[0]:0};entries=[];maps=[];map_ids={};hist_total=Counter();bygroup={};checks=0;captured_palettes=set()
 opacity_cache={};capture_groups={}
 for capture in captures:
  if capture['kind']!='intro_vram':continue
  raw=Path(capture['path']).read_bytes()
  active=set()
  for row in range(2,30):
   for col in range(4,44):
    code,attr=source_cell(raw,col,row,0x10000)
    if not 0x7000<=code<=0x7fff:continue
    key=code,col&1
    if key not in opacity_cache:opacity_cache[key]=any(p!=15 for row in decode_tile(gfx,code,8,col&1) for p in row)
    if opacity_cache[key]:active.add(attr&31)
  capture_groups[capture['frame']]=sorted(active)
  for group in active:
   actual=list(struct.unpack('>16H',raw[0x1c400+group*32:0x1c420+group*32]))
   assert actual==words[group],(capture['path'],group,'palette differs; cannot activate this fixed plan')
   captured_palettes.add((capture['path'],group))
 for code,group in keys:
  for layout in (0,1):
   pixels=decode_tile(gfx,code,8,layout);decoded[code,group,layout]=pixels;hist=Counter(colors[group][pen] for row in pixels for pen in row if pen!=15)
   look={color:[min((color_error(color,p),i) for i,p in enumerate(page)) for page in pages] for color in hist}
   page=min(range(4),key=lambda p:(sum(n*look[color][p][0] for color,n in hist.items()),p))
   pens=[0]+[1+min(range(15),key=lambda i:(color_error(colors[group][pen],pages[page][i]),i)) for pen in range(15)]
   key=(group,page,tuple(pens))
   if key not in map_ids:map_ids[key]=len(maps);maps.append({'source_group':group,'source_word_base':0x200+group*16,'source_words':words[group],'page':page,'pens':pens})
   tiles=[]
   for flip in range(4):
    oriented=[[0 if (pen:=pixels[7-y if flip&2 else y][7-x if flip&1 else x])==15 else pens[pen+1] for x in range(8)] for y in range(8)]
    packed=genesis_tiles(oriented)
    if packed not in ids:ids[packed]=len(patterns);patterns.append(packed)
    tiles.append(ids[packed])
    for y in range(8):
     for x in range(8):
      byte=packed[y*4+x//2];mapped=(byte>>(0 if x&1 else 4))&15;source=pixels[7-y if flip&2 else y][7-x if flip&1 else x]
      assert (mapped==0)==(source==15);assert mapped==(0 if source==15 else pens[source+1]);checks+=1
   entries.append({'code':code,'source_group':group,'layout':layout,'map':map_ids[key],'patterns_by_source_flip':tiles})
   stats=bygroup.setdefault(group,{'pixels':0,'squared_error':0,'weighted_error':0,'source_colors':set(),'output_colors':set(),'pages':Counter()})
   for color,n in hist.items():
    output_color=pages[page][look[color][page][1]];stats['pixels']+=n;stats['squared_error']+=n*sum((a-b)**2 for a,b in zip(color,output_color));stats['weighted_error']+=n*color_error(color,output_color);stats['source_colors'].add(color);stats['output_colors'].add(output_color);stats['pages'][page]+=n
 for group,stats in bygroup.items():
  stats['rgb_rmse']=math.sqrt(stats['squared_error']/max(1,3*stats['pixels']));stats['source_colors']=len(stats['source_colors']);stats['output_colors']=len(stats['output_colors'])
 blob=b''.join(patterns);output.parent.mkdir(parents=True,exist_ok=True);output.with_suffix('.bin').write_bytes(blob)
 report={'name':plan['name']+'_scroll1','cram':plan['cram'],'source_palette_words':words,'maps':maps,'entries':entries,'patterns':len(patterns),'pattern_bytes':len(blob),'binary_sha256':hashlib.sha256(blob).hexdigest(),'source_code_groups':len(keys),'code_group_layout_entries':len(entries),'verified_oriented_pixels':checks,'captured_source_palette_blocks_verified':len(captured_palettes),'capture_active_palette_groups':capture_groups,'per_group':bygroup,'captures':captures,'mapper_transparent_codes':excluded,'geometry':'Original8x8, source CPS layout=column&1, source flips explicitly keyed. No horizontal resampling or invented text positions.','scope':'Complete current enumerated font/overlay/HUD source key sets plus saved opening Scroll1 capture keys. Not full-game text/script coverage. Active source palette words must match live state before activation; unused future groups are not required until used. Target CRAM never changes here.'}
 output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');return {k:v for k,v in report.items() if k not in ('entries','maps','source_palette_words','cram','captures','per_group')}
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--plan',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);a=parser.parse_args();print(json.dumps(build(a.plan,a.output),indent=2))
