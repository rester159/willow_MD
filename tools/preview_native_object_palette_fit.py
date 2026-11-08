#!/usr/bin/env python3
"""Offline ROM-color comparisons; layers shown separately, no priority guess."""
from pathlib import Path
from collections import Counter,defaultdict
import argparse,json,re,zipfile,math
from PIL import Image,ImageDraw
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb,palette_words
from cps1_source_layers import SourceLayers
from native_palette_fit import color_error,nearest_hardware
from audit_native_object_palette_fit import cram_rgb
from fit_native_object_palette_anchors import is_neutral_stone,preserves_neutral

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit'
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--candidates',nargs=2,default=['joint_weight_1','joint_weight_16']);parser.add_argument('--prefix',default='palette');args=parser.parse_args()
report=json.loads((OUT/'object-palette-fit.json').read_text());program=assemble_maincpu(ROOT/'willow.zip')
with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
layers=SourceLayers(program,gfx)
names=['unchanged_background']+args.candidates
plans=[json.loads((OUT/f'object_palette_{name}.json').read_text()) for name in names]
header=(ROOT/'native/intro_probe/src/layers.h').read_text()
maps=[int(v,0) for v in re.findall('0x[0-9a-fA-F]+|\\b\\d+\\b',header.split('intro_maps[]=')[1].split(';')[0])]
patterns=(ROOT/'native/intro_probe/res/layers.bin').read_bytes()
bgwords={layer:[[w|0xf000 for w in palette_words(program,f'scroll{layer}',0,g)] for g in range(32)] for layer in (2,3)}
all_source_colors=set();actual_error=Counter();actual_n=0;actual_delta=[0,0,0];fitted_error=Counter();fitted_delta=[0,0,0];matched=0;different=0
sheet=Image.new('RGB',(1280,496),(24,24,24));draw=ImageDraw.Draw(sheet)
for col,label in enumerate(['CPS original full RGB','Current baked background']+args.candidates):draw.text((col*320+4,4),label,fill=(255,255,255))
for layer,origin,height,base in ((2,-752,736,0),(3,-496,480,3680)):
 for ty in range(height//8):
  for tx in range(40):
   source=[]
   for y in range(8):
    for x in range(8):
     pen,group=layers.pen(layer,32+tx*8+x,origin+ty*8+y)
     color=cps_rgb(bgwords[layer][group][pen]) if pen!=15 else None
     source.append(color)
     if color is not None:all_source_colors.add(color)
   pages=[];remaps=[]
   for plan in plans:
    pals=[[cram_rgb(w) for w in p[1:]] for p in plan['cram']]
    hist=Counter(c for c in source if c is not None)
    lookup={c:[min(((color_error(c,p),j) for j,p in enumerate(page) if not plan.get('neutral_stone_constraint') or not is_neutral_stone(c) or preserves_neutral(c,p)),default=None) for page in pals] for c in hist}
    allowed=[p for p in plan.get('background_pages',(2,3)) if all(lookup[c][p] is not None for c in hist)]
    page=min(allowed,key=lambda p:sum(n*lookup[c][p][0] for c,n in hist.items()))
    pages.append(page);remaps.append({c:pals[page][lookup[c][page][1]] for c in hist})
   encoded=maps[base+ty*40+tx];tid=encoded&32767;page=2+(encoded>>15)
   for y in range(8):
    for x in range(8):
     color=source[y*8+x];byte=patterns[tid*32+y*4+x//2];pen=(byte>>(0 if x&1 else 4))&15
     assert (color is None)==(pen==0)
     actual=cram_rgb(plans[0]['cram'][page][pen])
     if color is not None:
      actual_n+=1;actual_error['rgb']+=sum((a-b)**2 for a,b in zip(color,actual));actual_error['weighted']+=color_error(color,actual)
      fitted=remaps[0][color];fitted_error['rgb']+=sum((a-b)**2 for a,b in zip(color,fitted));fitted_error['weighted']+=color_error(color,fitted)
      if actual==fitted:matched+=1
      else:different+=1
      for channel in range(3):actual_delta[channel]+=actual[channel]-color[channel];fitted_delta[channel]+=fitted[channel]-color[channel]
     worldy=origin+ty*8+y
     if -240<=worldy<-16:
      py=worldy+240+24+(248 if layer==3 else 0);px=tx*8+x
      checker=(52,52,52) if ((px//8)+(py//8))&1 else (38,38,38)
      for col,output in enumerate([color,actual if color is not None else None,remaps[1].get(color),remaps[2].get(color)]):sheet.putpixel((col*320+px,py),output if output is not None else checker)
draw.text((4,250),'Scroll 3 shown separately; no object/layer priority inferred',fill=(255,255,255));sheet.save(OUT/f'{args.prefix}-background-comparison.png')
regions={}
for region,box,predicate in (
 ('canopy_green',(0,0,320,120),lambda c:c[1]>c[0] and c[1]>c[2]),
 ('stone_neutral',(0,125,127,190),lambda c:abs(c[0]-c[1])<=17 and abs(c[1]-c[2])<=34)):
 points=[(x,y+24) for y in range(box[1],box[3]) for x in range(box[0],box[2]) if predicate(sheet.getpixel((x,y+24)))]
 source=[sheet.getpixel(pos) for pos in points];values={}
 for col,name in enumerate(['source','current']+args.candidates):
  pixels=[sheet.getpixel((x+col*320,y)) for x,y in points];n=len(pixels)
  shift=[sum(c[k]-s[k] for s,c in zip(source,pixels))/n for k in range(3)]
  values[name]={'pixels':n,'visible_rgb_colors':len(set(pixels)),'rgb_rmse':math.sqrt(sum(sum((a-b)**2 for a,b in zip(s,c)) for s,c in zip(source,pixels))/(n*3)),'mean_rgb_shift':shift,'mean_green_minus_red_shift':shift[1]-shift[0],'mean_green_minus_blue_shift':shift[1]-shift[2]}
 regions[region]={'crop_xyxy':box,'selection':'Unchanged source RGB mask: G>R and G>B' if region=='canopy_green' else 'Unchanged source RGB mask: abs(R-G)<=17 and abs(G-B)<=34; approximate stone crop, not semantic material classification','results':values}
(OUT/f'{args.prefix}-region-metrics.json').write_text(json.dumps(regions,indent=2)+'\n')

entries=plans[0]['entries'];groups=(0,10,31);old=[cram_rgb(w) for w in json.loads((ROOT/'build/intro-sprite-assets.json').read_text())['palette'][1:]]
titles=['CPS original RGB','Old dedicated NPC page','Two players + current BG']+args.candidates
panelw=160;height=40+sum((math.ceil(sum(e['source_group']==g for e in entries)/8)*20+24) for g in groups)
sprites=Image.new('RGB',(panelw*5,height),(32,32,32));draw=ImageDraw.Draw(sprites)
for col,title in enumerate(titles):draw.text((col*panelw+2,4),title,fill='white')
groupy=32
for group in groups:
 groupentries=[e for e in entries if e['source_group']==group]
 for col in range(5):draw.text((col*panelw+2,groupy),f'CPS group {group}',fill='white')
 for n,e in enumerate(groupentries):
  code=e['code'];pixels=decode_tile(gfx,code,16);colors=[cps_rgb(w) for w in report['source_palette_words'][str(group)]]
  mapped=[]
  for plan in plans:
   pe=next(p for p in plan['entries'] if p['code']==code and p['source_group']==group);m=plan['maps'][pe['map']]
   mapped.append([cram_rgb(plan['cram'][m['page']][m['pens'][p+1]]) for p in range(15)])
  for y in range(16):
   for x in range(16):
    pen=pixels[y][x]
    if pen==15:continue
    color=colors[pen];prior=min(old,key=lambda p:color_error(color,p)) if group!=0 else nearest_hardware(color)
    outputs=[color,prior]+[p[pen] for p in mapped]
    for col,rgb in enumerate(outputs):sprites.putpixel((col*panelw+(n%8)*20+x,groupy+20+(n//8)*20+y),rgb)
 groupy+=math.ceil(len(groupentries)/8)*20+24
sprites.resize((sprites.width*2,sprites.height*2),Image.Resampling.NEAREST).save(OUT/f'{args.prefix}-object-comparison.png')
source_object={cps_rgb(w) for g in report['source_palette_words'].values() for w in g[:15]};object_q={nearest_hardware(c) for c in source_object};bg_q={nearest_hardware(c) for c in all_source_colors};bg_current={cram_rgb(w) for w in report['current_background_cram'][1:16]+report['current_background_cram'][17:32]}
evidence={'actual_baked_background':{'pixels':actual_n,'rgb_rmse':math.sqrt(actual_error['rgb']/(3*actual_n)),'chroma_weighted_mean_error':actual_error['weighted']/actual_n,'mean_rgb_shift':[v/actual_n for v in actual_delta]},'recomputed_baseline':{'rgb_rmse':math.sqrt(fitted_error['rgb']/(3*actual_n)),'chroma_weighted_mean_error':fitted_error['weighted']/actual_n,'mean_rgb_shift':[v/actual_n for v in fitted_delta],'pixels_matching_actual_baked':matched,'pixels_different':different},'unions':{'object_hardware_colors':len(object_q),'current_bg_hardware_colors':len(bg_current),'object_bg_hardware_reuse':len(object_q&bg_current),'object_plus_current_bg_hardware_union':len(object_q|bg_current),'all_bg_source_rgb':len(all_source_colors),'all_bg_individually_nearest_hardware':len(bg_q),'objects_plus_bg_individually_nearest_hardware':len(bg_q|object_q),'player_groups_0_and_1_hardware_union':len({nearest_hardware(cps_rgb(w)) for g in ('0','1') for w in report['source_palette_words'][g][:15]})},'color_lists':{'object_hardware':sorted(object_q),'current_background_hardware':sorted(bg_current),'background_individually_nearest_hardware':sorted(bg_q)},'images':['palette-background-comparison.png','palette-object-comparison.png'],'scope':'Offline decoded original ROM pixels and actual current baked background bytes. Separate layers/object sheets avoid inventing CPS priority. No video or actual cartridge comparison in this audit.'}
evidence['images']=[f'{args.prefix}-background-comparison.png',f'{args.prefix}-object-comparison.png'];evidence['candidates']=args.candidates
(OUT/f'{args.prefix}-preview-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps({k:v for k,v in evidence.items() if k!='color_lists'},indent=2))
