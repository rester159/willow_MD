#!/usr/bin/env python3
"""Read-only fine-Y dependency audit; no renderer/source behavior changes."""
import argparse
import csv
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import struct
import zipfile
from cps1_gfx import assemble_region,decode_tile
from prepare_native_route_background import cell_index

ROOT=Path(__file__).resolve().parents[1]

def first(y,mask):return (((16+y)&65535)>>3)&mask
def rows(y):return 28+bool(y&7)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=Path,default=ROOT/'build/native-background-fine-y-audit/v1')
 a=p.parse_args();out=a.output.resolve();assert not out.exists()
 source=ROOT/'tools/native_render_background.inc';pins={str(source):sha(source),str(Path(__file__).resolve()):sha(Path(__file__))}
 result=[]
 for layer,mask,scale in [(2,127,1),(3,255,2)]:
  checked=accepted=extent_changes=full_rows_change=0
  for old in range(65536):
   oy=first(old,mask);oldrows=rows(old)
   for delta in range(-7,8):
    if not delta:continue
    new=(old+delta)&65535;ny=first(new,mask);newrows=rows(new);checked+=1
    if oy==ny and oldrows==newrows:
     accepted+=1
     assert oy>>scale==ny>>scale
     # Every world/native row identity and subtile phase is exactly retained.
     assert all(((oy+r)&mask, (oy+r)&31, (oy+r)&((1<<scale)-1)) ==
                ((ny+r)&mask, (ny+r)&31, (ny+r)&((1<<scale)-1)) for r in range(oldrows))
    elif oy==ny:extent_changes+=1
    else:full_rows_change+=1
  result.append({'layer':layer,'adjacent_raw_u16_pairs':checked,'same_wrapped_row_and_extent':accepted,
                 'same_first_row_but_changed_extent':extent_changes,'changed_wrapped_first_row':full_rows_change})
 traces=[]
 for name in ['columns-v1','columns-v1-provenance','columns-v1-provenance-v2','columns-v1-stack-diagnostic-v2']:
  path=ROOT/'build/native-forest-extension'/name/'background-events.csv'
  if not path.exists():continue
  pins[str(path)]=sha(path)
  with path.open() as f:active=[r for r in csv.DictReader(f) if r['path']=='0' and r['phase']=='1' and r['active']=='1']
  views=sorted({(int(r['s2y']),int(r['s3y'])) for r in active})
  transitions=sum((x['s2y'],x['s3y'])!=(y['s2y'],y['s3y']) for x,y in zip(active,active[1:]))
  traces.append({'path':str(path),'active_preparation_entries':len(active),'distinct_y_views':views,'active_y_transitions':transitions})
 fixture=ROOT/'build/native-forest-extension/forest-v2/rejection-004-source.bin'
 raw=fixture.read_bytes();assert len(raw)==123392;pins[str(fixture)]=sha(fixture)
 archive=ROOT/'willow.zip';pins[str(archive)]=sha(archive)
 with zipfile.ZipFile(archive) as z:gfx=assemble_region(z)
 @lru_cache(None)
 def tile(layer,code):
  size=16 if layer==2 else 32
  pc=0x4000+code if layer==2 and code<=0x1fff else (code&0x3fff if layer==3 and 0xa00<=code&0x3fff<=0xdff else None)
  return decode_tile(gfx,pc,size) if pc is not None else [[15]*size for _ in range(size)]
 def opaque(layer,x,y):
  size=16 if layer==2 else 32;x%=size*64;y%=size*64
  at=(0 if layer==2 else 16384)+cell_index(layer,x//size,y//size)*4
  code,attr=struct.unpack_from('>HH',raw,at);im=tile(layer,code)
  return im[size-1-y%size if attr&64 else y%size][size-1-x%size if attr&32 else x%size]!=15
 def demand(np,fp,whole=False):
  cells=set();pixels=0
  for y in range(224):
   for x in (range(320) if whole else (319,)):
    if not opaque(2,96+132+x,16+768+np+y) and opaque(3,96+34+x,16+1792+fp+y):
     cells.add((((96+34+x)&2047)//8,((16+1792+fp+y)&2047)//8));pixels+=1
  return cells,pixels
 examples=[]
 candidates={}
 for np in range(1,8):
  for fp in range(1,8):candidates[np,fp]=demand(np,fp)
 for pair_a,a_value in candidates.items():
  for pair_b,b_value in candidates.items():
   if pair_a>=pair_b or a_value[0]==b_value[0]:continue
   a_cells,a_pixels=demand(*pair_a,True);b_cells,b_pixels=demand(*pair_b,True)
   if a_cells==b_cells:continue
   for axis,mask in [(0,127),(1,255)]:
    base=768 if axis==0 else 1792
    assert first(base+pair_a[axis],mask)==first(base+pair_b[axis],mask)
    assert rows(base+pair_a[axis])==rows(base+pair_b[axis])==29
   examples.append({'scope':'Hypothetical fine phases over unchanged actual source maps and original graphics; dependency counterexample, not a recorded game transition.',
       'fine_phases_a':pair_a,'fine_phases_b':pair_b,'far_pixels_a':a_pixels,'far_pixels_b':b_pixels,
       'required_far_world_cells_a':sorted(a_cells),'required_far_world_cells_b':sorted(b_cells),
       'pixel_checks':2*320*224,'both_layers_same_first_row_and_29_row_extent':True})
   break
  if examples:break
 assert examples,'Expected original-art counterexample to fine-Y far-culling reuse'
 report={'scope':__doc__,'renderer_modified':False,'native_timing_claim':False,'raw_u16_row_identity_proof':result,
         'source_trace_opportunity':traces,'far_culling_dependency_counterexample':examples,'inputs':pins}
 assert all(sha(Path(n))==d for n,d in pins.items())
 out.mkdir(parents=True);(out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='inputs'},indent=2))

if __name__=='__main__':main()
