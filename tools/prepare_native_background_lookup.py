#!/usr/bin/env python3
"""Compile an immutable full-source-key lookup, with no maps or coordinates."""
from pathlib import Path
import argparse,hashlib,json,struct
ROOT=Path(__file__).resolve().parents[1]
ORIGIN=0x140000

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def hashed(key,rotate,left,right,seed,mask):
 code,attr=key>>16,key&65535
 h=(code^((attr<<rotate)|(attr>>(16-rotate)))^seed)&65535
 h^=(h<<left)&65535
 h^=h>>right
 return h&mask

def table(keys,size,params):
 slots=[0]*size;probes=[]
 for index,key in enumerate(keys):
  at=hashed(key,*params,size-1);probe=1
  while slots[at]:at=(at+1)&(size-1);probe+=1
  slots[at]=index+1;probes.append(probe)
 return (max(probes),sum(probes)),slots,probes

def choose(keys,size):
 # Exhaustive three-shift search, then deterministic seed refinement of the
 # best24 mixes. All operations are16-bit; the target has no multiplication.
 candidates=[]
 for rotate in range(1,16):
  for left in range(1,16):
   for right in range(1,16):
    params=(rotate,left,right,0)
    candidates.append((table(keys,size,params)[0],params))
 candidates.sort();best=None
 for _,params in candidates[:24]:
  for seed in range(256):
   p=(*params[:3],seed);result=table(keys,size,p)
   value=(result[0],p)
   if best is None or value<best[0]:best=(value,result)
 return best[0][1],best[1]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--catalog',type=Path,default=ROOT/'build/native-background-catalog/prepared-v1')
 p.add_argument('--output',type=Path,default=ROOT/'build/native-background-lookup/prepared-v1')
 a=p.parse_args();catalog=a.catalog.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 manifest=json.loads((catalog/'manifest.json').read_text())
 for name,digest in manifest['outputs'].items():assert sha(catalog/name)==digest,name
 layout=json.loads((catalog/'layout.json').read_text());raw=(catalog/'catalog.bin').read_bytes()
 assert layout['origin']==0x120000 and len(raw)==layout['total_bytes']
 header=24;stride=24;offset=header+2*stride;descriptors=[];payload=bytearray();reports=[]
 for layer in layout['layers']:
  number=layer['layer'];count=layer['pair_count'];at=layer['offsets']['pairs'];size=1024 if number==2 else 512
  keys=[struct.unpack_from('>I',raw,at+i*6)[0]for i in range(count)]
  assert keys==sorted(set(keys)) and count<size
  for i in range(count):assert struct.unpack_from('>H',raw,at+i*6+4)[0]==i*layer['subtiles_per_pair']
  params,(cost,slots,probes)=choose(keys,size);rotate,left,right,seed=params
  assert cost[0]<=64
  descriptors.append(struct.pack('>HHI5H4BH',number,count,layout['origin']+at,size,size-1,cost[0],offset,seed,rotate,left,right,0,0))
  assert len(descriptors[-1])==stride
  for index,key in enumerate(keys):
   pos=hashed(key,*params,size-1)
   for n in range(cost[0]):
    value=slots[pos];assert value
    if keys[value-1]==key:assert value==index+1;break
    pos=(pos+1)&(size-1)
   else:raise AssertionError('known key absent')
  payload.extend(struct.pack('>'+str(size)+'H',*slots))
  reports.append({'layer':number,'pair_count':count,'pairs_address':layout['origin']+at,'slots':size,'mask':size-1,'slot_offset':offset,'slot_address':ORIGIN+offset,'max_probes':cost[0],'mean_known_probes':cost[1]/count,'total_known_probes':cost[1],'attr_rotate':rotate,'mix_left':left,'mix_right':right,'seed':seed,'probe_histogram':{str(n):probes.count(n)for n in sorted(set(probes))}})
  offset+=size*2
 blob=struct.pack('>IHHIIHHI',0x57424831,1,2,offset,24,24,1,layout['origin'])+b''.join(descriptors)+payload
 assert len(blob)==3144
 (out/'lookup.bin').write_bytes(blob)
 report={'magic':'WBH1','version':1,'origin':ORIGIN,'total_bytes':len(blob),'end_exclusive':ORIGIN+len(blob),'header_format':'>IHHIIHHI','header_fields':['magic','version','layer_count','total_bytes','directory_offset','descriptor_stride','hash_algorithm','catalog_origin'],'descriptor_format':'>HHI5H4BH','descriptor_fields':['layer','pair_count','pairs_address','slot_count','mask','max_probes','slot_offset','seed','attr_rotate','mix_left','mix_right','reserved8','reserved16'],'layers':reports,'semantics':'Slot0 empty; otherwise unchanged original catalogID+1. Linear probing with exact full32-bit source code/attribute comparison. All shifts/XOR/rotate are16-bit.'}
 (out/'layout.json').write_text(json.dumps(report,indent=2)+'\n')
 lines=['#ifndef WILLOW_BACKGROUND_LOOKUP_IMPORTS_H','#define WILLOW_BACKGROUND_LOOKUP_IMPORTS_H','#include "willow_native_background_lookup.h"',f'#define WILLOW_BG_LOOKUP_ORIGIN 0x{ORIGIN:08x}UL',f'#define WILLOW_BG_LOOKUP_BYTES {len(blob)}UL',f'#define WILLOW_BG_LOOKUP_CATALOG_BYTES {len(raw)}UL','static const WillowNativeBackgroundLookupIndex willowBackgroundLookupIndexes[2]={']
 for d in reports:
  lines.append(' {(const uint8_t*)0x%08xUL,(const uint8_t*)0x%08xUL,%d,%d,%d,%d,%d,%d,%d,0},'%(d['slot_address'],d['pairs_address'],d['pair_count'],d['mask'],d['max_probes'],d['seed'],d['attr_rotate'],d['mix_left'],d['mix_right']))
 lines+=['};','#endif',''];(out/'lookup_imports.h').write_text('\n'.join(lines))
 inputs=[Path(__file__).resolve(),catalog/'catalog.bin',catalog/'manifest.json',catalog/'layout.json']
 result={'scope':__doc__,'catalog_origin':layout['origin'],'origin':ORIGIN,'bytes':len(blob),'persistent_ram_bytes':0,'native_import_descriptor_rom_bytes':40,'hash_search':'all15^3 shiftmixes atseed0, thenall256seeds forbest24; lexicographic(maxprobe,totalprobes,params) objective','layers':reports,'inputs':{str(q):sha(q)for q in inputs},'outputs':{name:sha(out/name)for name in ['lookup.bin','lookup_imports.h','layout.json']}}
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'origin':ORIGIN,'bytes':len(blob),'layers':reports},indent=2))
if __name__=='__main__':main()
