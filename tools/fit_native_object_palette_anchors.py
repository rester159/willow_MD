#!/usr/bin/env python3
"""Bounded source-hue constrained palette experiment; never production admission.

The two original player pages remain unchanged. One of the background pages
retains explicit source-derived robe/wood anchors. An object piece containing
an anchored source pen must use that page and that pen's anchor. Backgrounds
still choose complete 8x8 pages and preserve transparent pixels.
"""
from pathlib import Path
from collections import Counter
from itertools import product
import argparse,hashlib,json,math,zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb,palette_words
from cps1_source_layers import SourceLayers
from native_palette_fit import color_error,fit_palette,nearest_hardware,LEVELS
from audit_native_object_palette_fit import cram_rgb,cram_word

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit'
BG_PAGES=(2,3)
NEUTRAL_STONE=False
def is_neutral_stone(color):return color[0]==color[1] and 0<=color[1]-color[2]<=17
def preserves_neutral(source,output):
 return abs((output[0]-output[1])-(source[0]-source[1]))<=17 and abs((output[1]-output[2])-(source[1]-source[2]))<=17
def quantize_anchor(source,kind):
 # Channel relations come from these original source words, not a tint.
 def preserves(p):return p[0]>p[2]>p[1] if kind=='robe' else p[0]>p[1]>p[2]
 candidates=[p for p in product(LEVELS,repeat=3) if preserves(p)]
 return min(candidates,key=lambda p:(color_error(source,p),sum((a-b)**2 for a,b in zip(source,p)),abs(sum(p)-sum(source)),p))
def anchored_palette(hist,anchors):
 colors=sorted(hist);palette=list(dict.fromkeys(anchors))
 candidates=sorted({nearest_hardware(c) for c in colors}|set(palette));fixed=len(palette)
 while len(palette)<min(15,len(candidates)):
  best={c:min(color_error(c,p) for p in palette) for c in colors}
  remaining=[p for p in candidates if p not in palette]
  palette.append(min(remaining,key=lambda p:(sum(hist[c]*min(best[c],color_error(c,p)) for c in colors),p)))
 for _ in range(6):
  groups=[Counter() for p in palette]
  for color in colors:groups[min(range(len(palette)),key=lambda j:color_error(color,palette[j]))][color]+=hist[color]
  updated=palette[:fixed]
  for old,h in zip(palette[fixed:],groups[fixed:]):
   total=sum(h.values());mean=tuple(sum(c[k]*n for c,n in h.items())/total for k in range(3)) if total else old
   updated.append(nearest_hardware(mean))
  if updated==palette:break
  palette=updated
 palette=list(dict.fromkeys(palette))
 while len(palette)<min(15,len(candidates)):
  best={c:min(color_error(c,p) for p in palette) for c in colors}
  palette.append(min((p for p in candidates if p not in palette),key=lambda p:(sum(hist[c]*min(best[c],color_error(c,p)) for c in colors),p)))
 return palette+[(0,0,0)]*(15-len(palette))
def analyze(items,palettes,anchor_page,anchors):
 colors={c for item in items for c in item['hist']}
 nearest={color:[min(page,key=lambda p:(color_error(color,p),page.index(p))) for page in palettes] for color in colors}
 background_nearest={}
 if NEUTRAL_STONE:
  for color in colors:
   background_nearest[color]=[]
   for page in palettes:
    candidates=[p for p in page if not is_neutral_stone(color) or preserves_neutral(color,p)]
    background_nearest[color].append(min(candidates,key=lambda p:color_error(color,p)) if candidates else None)
 result=[]
 for item in items:
  group=item.get('group',-1);fixed=anchors.get(group,{})
  forced=any(color in fixed for color in item['hist'])
  allowed=(group,) if group in (0,1) else (BG_PAGES if group<0 else ((anchor_page,) if forced else (0,1,2,3)))
  lookup=background_nearest if NEUTRAL_STONE and group<0 else nearest
  allowed=[page for page in allowed if all(lookup[color][page] is not None for color in item['hist'])]
  assert allowed,'No page can preserve required source neutral hues'
  mapped={page:{color:(fixed[color] if page==anchor_page and color in fixed else lookup[color][page]) for color in item['hist']} for page in allowed}
  page=min(allowed,key=lambda p:(sum(n*color_error(color,mapped[p][color]) for color,n in item['hist'].items()),p))
  result.append((page,mapped[page]))
 return result
def metrics(items,assigned):
 n=err=weighted=0;delta=[0,0,0];src=set();dest=set()
 for item,(_,mapping) in zip(items,assigned):
  for color,count in item['hist'].items():
   output=mapping[color];n+=count;src.add(color);dest.add(output);err+=count*sum((a-b)**2 for a,b in zip(color,output));weighted+=count*color_error(color,output)
   for k in range(3):delta[k]+=count*(output[k]-color[k])
 return {'pixels':n,'source_colors':len(src),'output_colors':len(dest),'rgb_rmse':math.sqrt(err/max(1,3*n)),'chroma_weighted_mean_error':weighted/max(1,n),'mean_rgb_shift':[v/max(1,n) for v in delta]}
def fit(bg,npc,initial,anchor_page,anchors,weight):
 palette=[list(p) for p in initial];anchor_colors=sorted({p for group in anchors.values() for p in group.values()});best=None
 # Admission to iteration zero already forces the page to contain anchors.
 base=analyze(bg,palette,anchor_page,{})
 hist=Counter()
 for item,(page,_) in zip(bg,base):
  if page==anchor_page:hist.update(item['hist'])
 for item in npc:hist.update({c:n*weight for c,n in item['hist'].items()})
 palette[anchor_page]=anchored_palette(hist,anchor_colors)
 for iteration in range(8):
  assignment=analyze(bg+npc,palette,anchor_page,anchors);h=[Counter(),Counter()]
  bgm=metrics(bg,assignment[:len(bg)]);npm=metrics(npc,assignment[len(bg):]);objective=bgm['chroma_weighted_mean_error']*bgm['pixels']+weight*npm['chroma_weighted_mean_error']*npm['pixels']
  if best is None or objective<best[0]:best=(objective,[list(p) for p in palette],iteration)
  for i,(item,(page,_)) in enumerate(zip(bg+npc,assignment)):
   if page>=2:
    for c,n in item['hist'].items():h[page-2][c]+=n*(weight if i>=len(bg) else 1)
  next_palette=palette[:2]+[anchored_palette(h[p-2],anchor_colors) if p==anchor_page else fit_palette(h[p-2]) for p in (2,3)]
  if next_palette==palette:break
  palette=next_palette
 return best[1],best[2]
def main():
 global BG_PAGES,NEUTRAL_STONE
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--reuse-player-pages',action='store_true');parser.add_argument('--neutral-stone',action='store_true');args=parser.parse_args()
 BG_PAGES=(0,1,2,3) if args.reuse_player_pages else (2,3);NEUTRAL_STONE=args.neutral_stone
 prefix='reuse' if args.reuse_player_pages else 'anchored'
 baseline=json.loads((OUT/'object-palette-fit.json').read_text());catalog=json.loads((OUT/'opening_object_tiles.json').read_text());source_words=baseline['source_palette_words']
 oldplan=json.loads((OUT/'object_palette_unchanged_background.json').read_text());initial=[[cram_rgb(w) for w in page[1:]] for page in oldplan['cram']]
 program=assemble_maincpu(ROOT/'willow.zip')
 with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
 layers=SourceLayers(program,gfx);bg=[]
 for layer,origin,height in ((2,-752,736),(3,-496,480)):
  words=[[w|0xf000 for w in palette_words(program,f'scroll{layer}',0,g)] for g in range(32)]
  for ty in range(height//8):
   for tx in range(40):
    hist=Counter()
    for y in range(8):
     for x in range(8):
      pen,group=layers.pen(layer,32+tx*8+x,origin+ty*8+y)
      if pen!=15:hist[cps_rgb(words[group][pen])]+=1
    bg.append({'hist':hist,'layer':layer,'x':32+tx*8,'y':origin+ty*8})
 objects=[]
 for e in oldplan['entries']:
  pixels=decode_tile(gfx,e['code'],16);colors=[cps_rgb(w) for w in source_words[str(e['source_group'])]]
  objects.append({'code':e['code'],'group':e['source_group'],'hist':Counter(colors[p] for row in pixels for p in row if p!=15)})
 anchors={};anchor_evidence=[]
 for group,pens,kind in ((10,(8,9,10),'robe'),(31,(2,3,4,5,6,7),'wood')):
  anchors[group]={}
  for pen in pens:
   word=source_words[str(group)][pen];source=cps_rgb(word);hardware=quantize_anchor(source,kind);anchors[group][source]=hardware
   anchor_evidence.append({'group':group,'source_pen':pen,'source_word':word,'source_rgb':source,'hardware_rgb':hardware,'cram':cram_word(hardware),'constraint':kind,'original_color_error':color_error(source,hardware)})
 npc=[o for o in objects if o['group']>=2];raw=(OUT/'opening_object_tiles.bin').read_bytes();raw_entries=dict(catalog['entries']);results=[]
 for page in (2,3):
  for weight in (1,4,16):
   name=f'{prefix}_p{page}_w{weight}';print('Fitting',name,flush=True);pal,iteration=fit(bg,npc,initial,page,anchors,weight)
   bgassigned=analyze(bg,pal,page,anchors);objassigned=analyze(objects,pal,page,anchors);npcassigned=analyze(npc,pal,page,anchors)
   forest=[(item,a) for item,a in zip(bg,bgassigned) if item['y']>=-240]
   metrics_row={'name':name,'anchor_page':page,'weight':weight,'iteration':iteration,'cram':[[0]+[cram_word(c) for c in p] for p in pal],'background':metrics(bg,bgassigned),'forest':metrics([v[0] for v in forest],[v[1] for v in forest]),'npc':metrics(npc,npcassigned),'background_page_counts':dict(Counter(a[0] for a in bgassigned)),'per_group':{str(g):metrics([o for o in objects if o['group']==g],[a for o,a in zip(objects,objassigned) if o['group']==g]) for g in (0,10,31)}}
   maps=[];ids={};entries=[];blocks=[bytes(128)];block_ids={blocks[0]:0};forced_pieces=0
   for obj,(page_selected,lookup) in zip(objects,objassigned):
    group=obj['group'];pens=[0]
    for pen in range(15):
     color=cps_rgb(source_words[str(group)][pen])
     output=lookup.get(color)
     if output is None:output=min(pal[page_selected],key=lambda p:color_error(color,p))
     pens.append(1+pal[page_selected].index(output))
    key=(group,page_selected,tuple(pens))
    if key not in ids:ids[key]=len(maps);maps.append({'source_group':group,'source_words':source_words[str(group)],'page':page_selected,'pens':pens})
    source_block=raw_entries[obj['code']];chunk=raw[source_block*128:(source_block+1)*128];chunk=bytes(pens[v>>4]<<4|pens[v&15] for v in chunk)
    if chunk not in block_ids:block_ids[chunk]=len(blocks);blocks.append(chunk)
    entries.append({'code':obj['code'],'source_group':group,'source_block':source_block,'variant_block':block_ids[chunk],'map':ids[key]})
    if any(c in anchors.get(group,{}) for c in obj['hist']):assert page_selected==page;forced_pieces+=1
   blob=b''.join(blocks);plan={'name':name,'source_palettes':source_words,'source_binary_sha256':catalog['binary_sha256'],'cram':metrics_row['cram'],'maps':maps,'entries':entries,'blocks':len(blocks),'binary_sha256':hashlib.sha256(blob).hexdigest(),'anchors':anchor_evidence,'anchor_page':page,'background_pages':BG_PAGES,'neutral_stone_constraint':NEUTRAL_STONE,'scope':'Constrained source-hue candidate, captured opening only. Both player pages unchanged. Unadmitted until visual and actual-cartridge checks.'}
   assert plan['cram'][:2]==oldplan['cram'][:2]
   (OUT/f'object_palette_{name}.json').write_text(json.dumps(plan,indent=2)+'\n');(OUT/f'object_palette_{name}.bin').write_bytes(blob)
   metrics_row.update(blocks=len(blocks),maps=len(maps),forced_object_pieces=forced_pieces);results.append(metrics_row);print(name,'bg',metrics_row['background']['rgb_rmse'],'forest',metrics_row['forest']['rgb_rmse'],'npc',metrics_row['npc']['rgb_rmse'],flush=True)
 report={'anchors':anchor_evidence,'anchor_hardware_colors':len({a['cram'] for a in anchor_evidence}),'background_tile_count':len(bg),'background_pages':BG_PAGES,'neutral_stone_constraint':{'enabled':NEUTRAL_STONE,'selection':'Original background RGB R=G and 0<=G-B<=17','rule':'Each output R-G and G-B differs from source by at most17 (half one Genesis channel step)'},'object_pieces':len(objects),'candidates':results,'scope':'Fixed two player pages. Nine original source pens constrained to preserve original channel ordering, deduplicated anchor slots; remaining slots fitted with full source RGB and existing chroma-sensitive error. Not a global optimum or shipping admission.'}
 (OUT/('object-palette-reuse-fit.json' if args.reuse_player_pages else 'object-palette-constrained-fit.json')).write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
