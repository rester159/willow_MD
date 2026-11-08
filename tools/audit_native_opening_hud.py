#!/usr/bin/env python3
"""Read-only live Scroll1/HUD footprint, palette and Window feasibility audit."""
from pathlib import Path
from collections import Counter
import argparse,csv,hashlib,json,struct,sys,zipfile
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb
from native_palette_fit import color_error
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-opening-hud-audit/v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 context=ROOT/'build/native-background-context/content-v2';layout=json.loads((context/'layout.json').read_text());rows=list(csv.DictReader((context/layout['index']).open()))
 old=json.loads((ROOT/'build/cartridge-gap-audit/scroll1_reuse_p3_w16.json').read_text());oldkeys={(e['code'],e['source_group'],e['layout']):e for e in old['entries']}
 cram=struct.unpack('>64H',(ROOT/'build/native-route-objects/prepared-v2/cram.bin').read_bytes());pages=[[tuple(((w>>s)&7)*34 for s in(1,5,9))for w in cram[i*16:(i+1)*16]]for i in range(4)]
 oldcram=tuple(w for page in old['cram']for w in page)
 with zipfile.ZipFile(ROOT/'willow.zip')as z:gfx=assemble_region(z)
 decoded={};used={};snapshots=[];palettes={};allkeys=set();active=0
 with(context/layout['binary']).open('rb')as f:
  for index,row in enumerate(rows):
   if row['manager']!='2'or row['scene']!='2':continue
   active+=1;f.seek(index*layout['record_bytes']);raw=f.read(layout['record_bytes']);m=raw[32768:49152];pal=raw[49152:55296];ports=raw[55296:55808]
   w=lambda at:struct.unpack_from('>H',ports,at)[0]
   assert w(0x102)==0x9100
   col=((96+w(0x10c))&511)//8;sr=((16+w(0x10e))&511)//8
   assert (96+w(0x10c))%8==0 and(16+w(0x10e))%8==0
   count=0;bands=Counter();cells=[]
   for y in range(28):
    for x in range(40):
     cx=(col+x)&63;cy=(sr+y)&63;at=4*((cy&31)+(cx<<5)+((cy&32)<<6));code,attr=struct.unpack_from('>HH',m,at)
     if not 0x7000<=code<=0x7fff:continue
     key=(code,attr,cx&1);dkey=(code,cx&1)
     if dkey not in decoded:decoded[dkey]=decode_tile(gfx,code,8,cx&1)
     pixels=decoded[dkey]
     if not any(pen!=15 for r in pixels for pen in r):continue
     count+=1;bands[y]+=1;allkeys.add(key);cells.append([x,y,code,attr,cx&1]);group=attr&31;words=struct.unpack_from('>16H',pal,0x400+group*32)
     if group in palettes:assert palettes[group]==words
     palettes[group]=words;used[key]=pixels
   first=min(bands,default=28);last=max(bands,default=-1)+1;window=[0,last]if last<=28-first else[first,28]
   snapshots.append({'sample':index,'frame':int(row['frame']),'capture':int(row['capture']),'count':count,'opaque_rows':dict(bands),'window_rows':window,'source_column':col,'source_row':sr,'ports':{hex(at):w(at)for at in[0x102,0x10c,0x10e,0x122,0x170,0x16e,0x16c,0x16a,0x168]},'cells':cells})
 patterns={bytes(32):0};entries=[];missing=[];oldpal=[]
 for key,pixels in sorted(used.items()):
  code,attr,layout_id=key;group=attr&31;colors=[cps_rgb(w)for w in palettes[group]];hist=Counter(colors[pen]for r in pixels for pen in r if pen!=15)
  choices=[]
  for page in range(4):
   mapping={color:min(range(1,16),key=lambda pen:(color_error(color,pages[page][pen]),pen))for color in hist}
   choices.append((sum(n*color_error(color,pages[page][mapping[color]])for color,n in hist.items()),page,mapping))
  cost,page,mapping=min(choices,key=lambda item:(item[0],item[1]));mapped=[[0 if(pen:=pixels[7-y if attr&64 else y][7-x if attr&32 else x])==15 else mapping[colors[pen]]for x in range(8)]for y in range(8)];packed=genesis_tiles(mapped)
  if packed not in patterns:patterns[packed]=len(patterns)
  oldentry=oldkeys.get((code,group,layout_id))
  if oldentry is None:missing.append(key)
  elif tuple(old['maps'][oldentry['map']]['source_words'])!=palettes[group]:oldpal.append(key)
  entries.append({'code':code,'source_attribute':attr,'layout':layout_id,'pattern':patterns[packed],'page':page,'fit_weighted_error':cost})
 source=ROOT/'build/native-object-publication/checkpoint-visual-state-v1';writes=list(csv.DictReader((source/'scroll1-writes.csv').open()));vis=list(csv.DictReader((source/'visual-contexts.csv').open()));activefirst=min(int(r['master_cycles'])for r in vis if r['manager']==r['scene']=='2');sites=Counter();ranges={}
 for r in writes:
  if int(r['master_cycles'])<activefirst:continue
  pc=int(r['callback_pc'],16);at=int(r['address'],16);sites[pc]+=1;ranges.setdefault(pc,[at,at]);ranges[pc][0]=min(at,ranges[pc][0]);ranges[pc][1]=max(at,ranges[pc][1])
 evidence={'scope':__doc__,'boundary':layout['boundary'],'active_snapshots':active,'distinct_visible_maps':len({json.dumps(s['cells'])for s in snapshots}),'opaque_rows':sorted({int(r)for s in snapshots for r in s['opaque_rows']}),'window_extents':sorted({tuple(s['window_rows'])for s in snapshots}),'visible_nonblank_cells':{'min':min(s['count']for s in snapshots),'max':max(s['count']for s in snapshots)},'source_groups':sorted(palettes),'source_palette_words':palettes,'observed_visible_code_fullattr_layout_keys':len(entries),'existing_scroll1_key_misses':missing,'existing_scroll1_palette_misses':oldpal,'existing_scroll1_cram_differences':[{'slot':i,'old':oldcram[i],'current':cram[i]}for i in range(64)if oldcram[i]!=cram[i]],'fresh_fit_estimate':{'nonblank_patterns':len(patterns)-1,'bytes':(len(patterns)-1)*32,'reserved_patterns':105,'fits_reserved':len(patterns)-1<=105,'scope':'Count-only read-only nearest-current-CRAM fit preserving full source RGB and original geometry; not admitted art or a visual check. Both player CRAM pages are read without edits.'},'old_route_active_write_sites':[{'callback_pc':hex(pc),'writes':count,'first':hex(ranges[pc][0]),'last':hex(ranges[pc][1])}for pc,count in sorted(sites.items())],'inputs':{str(p):sha(p)for p in[Path(__file__),context/'layout.json',context/layout['index'],context/layout['binary'],ROOT/'willow.zip',ROOT/'build/cartridge-gap-audit/scroll1_reuse_p3_w16.json',ROOT/'build/native-route-objects/prepared-v2/cram.bin',source/'scroll1-writes.csv',source/'visual-contexts.csv']}}
 (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');(out/'visible-snapshots.json').write_text(json.dumps(snapshots,indent=2)+'\n');(out/'source-keys.json').write_text(json.dumps(entries,indent=2)+'\n');print(json.dumps({k:v for k,v in evidence.items()if k not in['inputs','source_palette_words','old_route_active_write_sites']},indent=2))
if __name__=='__main__':main()
