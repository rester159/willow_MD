#!/usr/bin/env python3
"""Independent original-pixel proof for clipped near edges and byte mask overlap."""
from pathlib import Path
from functools import lru_cache
import argparse,hashlib,json,struct,zipfile
from cps1_gfx import assemble_region,decode_tile
from prepare_native_route_background import cell_index,physical_code
from test_native_background_context import REGIONS
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def edge_full(flags,masks,fine):
 count=sum(f!=1 for f in flags)
 for c in [0,42]+([41]if not fine else[]):count-=flags[c]!=1
 if fine:
  for c,slice in [(1,255>>fine),(41,(255<<(8-fine))&255)]:
   if flags[c]!=1 and all((row&slice)==slice for row in masks[c]):count-=1
 return count==0

def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-visible-opacity/edges-v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 cases=0
 for fine in range(8):
  for col in range(43):
   for y in range(8):
    for x in range(8):
     flags=[1]*43;masks=[[255]*8 for _ in range(43)];flags[col]=0;masks[col][y]^=0x80>>x
     sx=col*8-8-fine+x;expected=not(0<=sx<320)
     assert edge_full(flags,masks,fine)==expected;cases+=1
 # Includes two partially-visible edge tiles, both all-opacity slices, then
 # admits neither if even one on-screen pixel is transparent.
 for fine in range(1,8):
  flags=[1]*43;masks=[[255]*8 for _ in range(43)]
  for col,slice in [(1,255>>fine),(41,(255<<(8-fine))&255)]:flags[col]=0;masks[col]=[slice]*8
  assert edge_full(flags,masks,fine);cases+=1
  masks[1][7]^=1;assert not edge_full(flags,masks,fine);cases+=1
 paths=[ROOT/'build/native-forest-extension/v8-first-boundary-frozen/rejection-004.json',ROOT/'build/native-forest-extension/forest-v2/successful-view130.json',ROOT/'build/native-forest-extension/forest-v2/rejection-004.json']
 zp=ROOT/'willow.zip'
 with zipfile.ZipFile(zp)as z:gfx=assemble_region(z)
 @lru_cache(None)
 def image(layer,code):
  size=16 if layer==2 else 32;physical=physical_code(layer,code)
  return decode_tile(gfx,physical,size)if physical is not None else[[15]*size for _ in range(size)]
 results=[];pins={str(Path(__file__).resolve()):sha(Path(__file__)),str(zp):sha(zp)}
 for path in paths:
  meta=json.loads(path.read_text());bp=path.with_suffix('.bin')if path.stem.endswith('-source')else path.with_name(path.stem+'-source.bin');raw=bp.read_bytes();pins[str(path)]=sha(path);pins[str(bp)]=sha(bp)
  regions={};off=0
  for name,size,address in REGIONS:regions[name]=raw[off:off+size];off+=size
  assert off==len(raw)==123392;v=meta['views']
  @lru_cache(None)
  def subtile(layer,x,y):
   size=16 if layer==2 else 32;scale=size//8;x%=size*8;y%=size*8;at=cell_index(layer,x//scale,y//scale)*4;code,attr=struct.unpack_from('>HH',regions[f'scroll{layer}'],at);im=image(layer,code);sx=(x%scale)*8;sy=(y%scale)*8;mask=[]
   for yy in range(8):
    byte=0
    for xx in range(8):
     qx=sx+xx;qy=sy+yy
     if im[size-1-qy if attr&64 else qy][size-1-qx if attr&32 else qx]!=15:byte|=0x80>>xx
    mask.append(byte)
   return 1 if all(m==255 for m in mask)else 2 if not any(mask)else 0,mask
  starts=[((((96+v[l*2])&65535)>>3)-1)&(255 if l else 127)for l in range(2)];tops=[(((16+v[l*2+1])&65535)>>3)&(255 if l else 127)for l in range(2)];rows=[28+bool(v[l*2+1]&7)for l in range(2)];counts=[[0]*32 for _ in range(2)];edge={};changed=[]
  for l in range(2):
   for row in range(rows[l]):
    y=(tops[l]+row)&(255 if l else 127);cells=[subtile(l+2,starts[l]+c,y)for c in range(43)];flags=[z[0]for z in cells];masks=[z[1]for z in cells];counts[l][y&31]=sum(f!=(2 if l else 1)for f in flags)
    if not l:
     edge[y&31]=edge_full(flags,masks,v[0]&7)
     whole=sum(f!=1 for f in flags)-sum(flags[c]!=1 for c in[0,42]+([41]if not(v[0]&7)else[]))==0
     if edge[y&31] and not whole:changed.append({'world_row':y,'native_row':y&31,'nonfull_columns':[i for i,f in enumerate(flags)if f!=1]})
  blank=True
  for row in range(rows[1]):
   y=(tops[1]+row)&255
   if not counts[1][y&31]:continue
   lo=max(0,row*8-(v[3]&7));hi=min(223,row*8-(v[3]&7)+7)
   if any(not edge[((16+v[1]+s)>>3)&31]for s in(lo,hi)):blank=False
  exposed=[];transparent=0;byte_calls=0;byte_wanted=[]
  for y in range(224):
   for x in range(320):
    nx=(96+v[0]+x)&1023;ny=(16+v[1]+y)&1023
    if subtile(2,nx>>3,ny>>3)[1][ny&7]&(0x80>>(nx&7)):continue
    transparent+=1;fx=(96+v[2]+x)&2047;fy=(16+v[3]+y)&2047
    if subtile(3,fx>>3,fy>>3)[1][fy&7]&(0x80>>(fx&7)):exposed.append([x,y])
  for row in range(rows[1]):
   for col in range(43):
    sx=col*8-8-(v[2]&7);sy=row*8-(v[3]&7);far=subtile(3,starts[1]+col,tops[1]+row)[1];need=False
    for yy in range(8):
     if not(0<=sy+yy<224):continue
     visible=sum(0x80>>xx for xx in range(8)if 0<=sx+xx<320);f=far[yy]&visible
     if not f:continue
     nx=(96+v[0]+sx)&1023;ny=(16+v[1]+sy+yy)&1023;shift=nx&7;a0=subtile(2,nx>>3,ny>>3)[1][ny&7];a1=subtile(2,(nx>>3)+1,ny>>3)[1][ny&7]
     overlap=((a0<<shift)&255)|(a1>>(8-shift)if shift else 0);need|=bool(f&~overlap);byte_calls+=1
    oracle=any(sx<=x<sx+8 and sy<=y<sy+8 for x,y in exposed)
    assert need==oracle
    if need:byte_wanted.append([col,row])
  assert not blank or not exposed
  if v[0] in(120,130):assert blank and not exposed
  else:assert not blank and exposed==[[319,112]]
  fixture=bytearray(struct.pack('>4H',*v));all_masks=[]
  for l in range(2):
   scale=4 if l else 2;w=12 if l else 22;h=8 if l else 15;rx=starts[l]//scale;ry=tops[l]//scale
   for my in range(h):
    for mx in range(w):
     for iy in range(scale):
      for ix in range(scale):
       flag,mask=subtile(l+2,((rx+mx)&63)*scale+ix,((ry+my)&63)*scale+iy);fixture.append(flag);all_masks.extend(mask)
  fixture.extend(all_masks);assert len(fixture)==25712
  fp=out/('source-masks-view'+str(v[0])+'.bin');fp.write_bytes(fixture)
  results.append({'view':v,'frame':meta['frame'],'clipped_row_far_blank':blank,'near_transparent_pixels':transparent,'exposed_far_pixels':exposed,'byte_overlap_required_cells':byte_wanted,'byte_overlap_row_checks':byte_calls,'newly_proved_clipped_rows':changed,'fixture':str(fp),'fixture_sha256':sha(fp)})
 evidence={'scope':__doc__,'exhaustive_single_transparent_pixel_and_edge_pair_cases':cases,'source_pixel_checks':71680*len(paths),'captures':results,'inputs':pins};(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps({k:v for k,v in evidence.items()if k!='inputs'},indent=2))
if __name__=='__main__':main()
