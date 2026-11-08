#!/usr/bin/env python3
"""Independent source-pixel proof for the visible-width near-opacity shortcut.
No renderer edits, target writes, cycle or live-presentation admission.
"""
from pathlib import Path
from collections import Counter
from functools import lru_cache
import argparse,hashlib,json,random,struct,zipfile
from cps1_gfx import assemble_region,decode_tile
from prepare_native_route_background import cell_index,physical_code
from test_native_background_context import REGIONS
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def near_full(flags,fine):
 count=sum(v!=1 for v in flags)
 count-=flags[0]!=1;count-=flags[42]!=1
 if not fine:count-=flags[41]!=1
 return count==0

def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-visible-opacity/v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 checked=0;rand=random.Random(0x57494c4c);single=[]
 # Exhaust every possible position of a lone nonfull source subtile and all
 # fine phases. The count is additive: removing exactly the disjoint guard
 # terms leaves precisely the sum over visible-intersecting source tiles.
 for fine in range(8):
  visible=[c for c in range(43)if c*8-8-fine<320 and c*8-fine>0]
  expected=list(range(1,41 if fine==0 else 42));assert visible==expected
  for hole in range(43):
   f=[1]*43;f[hole]=2
   assert near_full(f,fine)==(hole not in visible);checked+=1
  for pattern in range(32):
   f=[1]*43
   for bit,c in enumerate((0,1,40,41,42)):
    if pattern&(1<<bit):f[c]=0
   assert near_full(f,fine)==all(f[c]==1 for c in visible);checked+=1
  for n in range(4096):
   f=[rand.choice((0,1,1,1,2))for _ in range(43)]
   assert near_full(f,fine)==all(f[c]==1 for c in visible);checked+=1
  single.append({'fine_x':fine,'visible_intersecting_columns':[visible[0],visible[-1]],'removed':[0,42]+([41]if not fine else[])})
 # Native rowCounts indexes do NOT uniquely identify world Y. Verify the
 # reconstruction for every128-world origin, every fine phase and every row.
 world_checks=0
 for first in range(128):
  for fine in range(8):
   rows=28+bool(fine)
   for dy in range(rows):
    actual=(first+dy)&127;native=actual&31
    rebuilt=(first+((native-first)&31))&127
    assert rebuilt==actual;world_checks+=1
 # Counterexample for retaining old farBlank when only near fineX changes.
 f=[1]*43;f[41]=0
 assert near_full(f,0) and not near_full(f,1)
 # This doesn't authorize partial edge pixels: nonFULL edge tiles can fail
 # even if their holes are offscreen; refusal remains conservative.
 paths=[ROOT/'build/native-forest-extension/v8-first-boundary-frozen/rejection-004.json',ROOT/'build/native-forest-extension/forest-v1-first-far/rejection-004.json']
 zp=ROOT/'willow.zip'
 with zipfile.ZipFile(zp)as z:gfx=assemble_region(z)
 @lru_cache(None)
 def image(layer,code):
  size=16 if layer==2 else 32;physical=physical_code(layer,code)
  return decode_tile(gfx,physical,size)if physical is not None else[[15]*size for _ in range(size)]
 captures=[];pins={str(Path(__file__).resolve()):sha(Path(__file__)),str(zp):sha(zp)}
 for path in paths:
  meta=json.loads(path.read_text());bp=path.with_name(path.stem+'-source.bin');raw=bp.read_bytes();pins[str(path)]=sha(path);pins[str(bp)]=sha(bp)
  regions={};at=0
  for name,size,address in REGIONS:regions[name]=raw[at:at+size];at+=size
  assert at==len(raw)==123392;v=meta['views']
  @lru_cache(None)
  def subtile(layer,x,y):
   size=16 if layer==2 else 32;scale=size//8;x%=size*8;y%=size*8;off=cell_index(layer,x//scale,y//scale)*4;code,attr=struct.unpack_from('>HH',regions[f'scroll{layer}'],off);im=image(layer,code);sx=(x%scale)*8;sy=(y%scale)*8
   masks=[]
   for yy in range(8):
    mask=0
    for xx in range(8):
     sourceX=sx+xx;sourceY=sy+yy;pen=im[size-1-sourceY if attr&64 else sourceY][size-1-sourceX if attr&32 else sourceX]
     if pen!=15:mask|=0x80>>xx
    masks.append(mask)
   flag=1 if all(m==255 for m in masks)else 2 if not any(masks)else 0
   return flag,masks
  first=[((((96+v[l*2])&65535)>>3)-1)&(255 if l else 127)for l in range(2)];top=[(((16+v[l*2+1])&65535)>>3)&(255 if l else 127)for l in range(2)];counts=[[0]*32 for _ in range(2)];rows=[28+bool(v[l*2+1]&7)for l in range(2)];near_rows={};differences=[]
  for layer in range(2):
   for row in range(rows[layer]):
    y=(top[layer]+row)&(255 if layer else 127);flags=[subtile(layer+2,first[layer]+c,y)[0]for c in range(43)]
    counts[layer][y&31]=sum(f!=(2 if layer else 1)for f in flags)
    if not layer:
     near_rows[y&31]=near_full(flags,v[0]&7)
     if counts[0][y&31] and near_rows[y&31]:differences.append({'native_row':y&31,'world_row':y,'nonfull_retained_columns':[c for c,f in enumerate(flags)if f!=1]})
  def blank(refined):
   for row in range(rows[1]):
    yy=(top[1]+row)&255
    if not counts[1][yy&31]:continue
    lo=max(0,row*8-(v[3]&7));hi=min(223,row*8-(v[3]&7)+7)
    for screen_y in(lo,hi):
     ny=((16+v[1]+screen_y)>>3)&31
     if counts[0][ny]and(not refined or not near_rows[ny]):return False
   return True
  transparent=0;required=[]
  for y in range(224):
   for x in range(320):
    nx=(96+v[0]+x)&1023;ny=(16+v[1]+y)&1023
    if subtile(2,nx>>3,ny>>3)[1][ny&7]&(0x80>>(nx&7)):continue
    transparent+=1;fx=(96+v[2]+x)&2047;fy=(16+v[3]+y)&2047
    if subtile(3,fx>>3,fy>>3)[1][fy&7]&(0x80>>(fx&7)):required.append([x,y])
  fixture=bytearray(struct.pack('>4H',*v))
  for layer in range(2):
   scale=4 if layer else 2;w=12 if layer else 22;h=8 if layer else 15;rx=(first[layer]//scale)&63;ry=(top[layer]//scale)&63
   for my in range(h):
    for mx in range(w):
     for suby in range(scale):
      for subx in range(scale):fixture.append(subtile(layer+2,((rx+mx)&63)*scale+subx,((ry+my)&63)*scale+suby)[0])
  assert len(fixture)==2864
  fixture_path=out/('source-flags-'+str(meta['active_input_ticks'])+'.bin');fixture_path.write_bytes(fixture)
  refined=blank(True);assert not refined or not required
  if meta['active_input_ticks']==277:assert refined and not required
  else:assert not refined and required==[[319,112]]
  captures.append({'tick':meta['active_input_ticks'],'frame':meta['frame'],'view':v,'old_far_blank':blank(False),'refined_far_blank':refined,'near_transparent_pixels':transparent,'visible_far_pixels':required,'guard_only_rows':differences,'all_pixel_checks':71680,'fixture':str(fixture_path),'fixture_sha256':sha(fixture_path)})
 result={'scope':__doc__,'count_additivity_cases':checked,'world_native_row_reconstruction_cases':world_checks,'all_fine_phases':single,'fine_only_invalidation_counterexample':{'nonfull_column':41,'fine0_blank':True,'fine1_blank':False,'required_action':'Recheck refined proof on near fineX changes even if no8px ring move or journal entries.'},'captures':captures,'preconditions':['rowCounts is exact over CURRENT43retained columns and current ring after journal/entering-column updates','FULL flag derives from every original8x8source pixel, not fitted colors','Unknown ring/ref and inconsistent count cannot certify opacity','Near fullworld row is reconstructed before bgRef; rowCounts indexes remain nativeY&31','Partial visible edge tiles conservatively require complete source opacity'],'inputs':pins}
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='inputs'},indent=2))
if __name__=='__main__':main()
