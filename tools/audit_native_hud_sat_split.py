#!/usr/bin/env python3
"""Read-only opening HUD split feasibility. No target, assets, or source writes."""
from pathlib import Path
import argparse,csv,hashlib,json,struct,zipfile
from collections import Counter
from cps1_gfx import assemble_region,decode_tile
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-opening-hud-audit/v1');p.add_argument('--target',type=Path,default=ROOT/'build/native-background-publication/observer-live-v8');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 first=ROOT/'build/native-opening-hud-audit/v1/visible-snapshots.json';snapshots=json.loads(first.read_text());target=a.target.resolve()
 with zipfile.ZipFile(ROOT/'willow.zip')as z:gfx=assemble_region(z)
 pixels={}
 def opaque(code,layout):
  key=(code,layout)
  if key not in pixels:pixels[key]=sum(v!=15 for row in decode_tile(gfx,code,8,layout)for v in row)
  return pixels[key]
 # Independently check current v8 source maps; these are successful post-source
 # IRQ background publication snapshots, not the prior object's latch sample.
 publications=list(csv.DictReader((target/'background-publications.csv').open()));blob=(target/'background-source-contexts.bin').read_bytes();v8=[]
 assert len(blob)==len(publications)*123392
 for r in publications:
  i=int(r['sample']);b=blob[i*123392:(i+1)*123392];manager,scene=struct.unpack_from('>HH',b,57856+0x8004)[0],struct.unpack_from('>H',b,57856+0x8008)[0]
  if manager!=2 or scene!=2:continue
  port=b[55296:55808];s1=b[32768:49152];w=lambda n:struct.unpack_from('>H',port,n)[0]
  assert w(0x102)==0x9100 and (96+w(0x10c))%8==0 and (16+w(0x10e))%8==0
  col=((96+w(0x10c))&511)//8;row=((16+w(0x10e))&511)//8;cells=[]
  for y in range(28):
   for x in range(40):
    cx=(col+x)&63;cy=(row+y)&63;code,attr=struct.unpack_from('>HH',s1,4*((cy&31)+(cx<<5)+((cy&32)<<6)))
    if 0x7000<=code<=0x7fff and opaque(code,cx&1):cells.append([x,y,code,attr,cx&1])
  v8.append({'frame':int(r['frame']),'cells':cells})
 allstates={json.dumps(s['cells']):s for s in snapshots+v8};maxhud=[0]*224;maxtotal=0;lower=[]
 for s in allstates.values():
  top=[c for c in s['cells']if c[1]<26];line=Counter()
  for x,y,*_ in top:
   for yy in range(y*8,y*8+8):line[yy]+=1
  maxhud=[max(maxhud[y],line[y])for y in range(224)];maxtotal=max(maxtotal,len(top))
  counts={y:sum(opaque(c,l)for x,yy,c,a,l in s['cells']if yy==y)for y in(26,27)};lower.append(counts)
 assert all(c[26]==2560 and c[27]==2560 for c in lower)
 sat=(target/'render-sat.bin').read_bytes();index=list(csv.DictReader((target/'render-sat.csv').open()));assert len(sat)==640*len(index)
 total=scan=pix=0;objects=[]
 for r in index:
  i=int(r['sample']);raw=sat[i*640:(i+1)*640];n=0;seen=set();sc=[0]*224;px=[0]*224
  while n not in seen:
   seen.add(n);y,size,attr,x=struct.unpack_from('>4H',raw,n*8);y=(y&511)-128;x=(x&511)-128;w=(((size>>10)&3)+1)*8;h=(((size>>8)&3)+1)*8
   # Count full hardware-fetch width, even for transparent/clipped pixels.
   for yy in range(max(y,0),min(y+h,224)):sc[yy]+=1;px[yy]+=w
   n=size&127
   if not n:break
   assert n<80
  objects.append(len(seen));total=max(total,len(seen)+maxtotal);scan=max(scan,max(sc[y]+maxhud[y]for y in range(224)));pix=max(pix,max(px[y]+8*maxhud[y]for y in range(224)))
 result={'scope':__doc__,'reference_pre_irq_snapshots':len(snapshots),'target_successful_post_irq_bg_snapshots':len(v8),'combined_distinct_hud_maps':len(allstates),'lower_window':{'first_row':26,'end_row':28,'reg17':0,'reg18':154,'source_opaque_pixels_each_state':5120,'source_pixels_tested_each_state':5120,'native_map_words':80,'native_map_bytes':160,'vram_rows':['0xdd00..0xdd4f','0xdd80..0xddcf'],'precondition':'Every pixel must remain original-source opaque; otherwise refuse this split, never expose missing near background.'},'upper_sat':{'source_rows':'All visible cells outside Window, not a fixed HUD coordinate list. Observed native rows 0,1,24.','max_single_cell_pieces':maxtotal,'object_snapshots':len(index),'object_pieces_min':min(objects),'object_pieces_max':max(objects),'worst_combined_total':total,'worst_combined_scanline_sprites':scan,'worst_combined_scanline_fetch_pixels':pix,'fits_observed_hardware_budget':total<=80 and scan<=20 and pix<=320,'comparison':'Each captured actual target SAT tested against worst occupancy over every observed HUD state; no coalescing or object removal. Capacity proof only, no rendered pixel/timing admission.'},'new_ram_gap':'Current v8 BSS ends FF6FFC, only4 bytes free before native stacks. Full Window helper2252 + target112 does not fit. A compact400-byte five-row resolved cache plus lifecycle metadata still needs explicit owned storage; do not silently reuse source RAM or object fallback storage.','inputs':{str(p):sha(p)for p in[Path(__file__),first,ROOT/'willow.zip',target/'render-sat.csv',target/'render-sat.bin',target/'background-publications.csv',target/'background-source-contexts.bin']}}
 (out/'split-feasibility.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='inputs'},indent=2))
if __name__=='__main__':main()
