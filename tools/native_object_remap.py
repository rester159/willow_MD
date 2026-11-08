#!/usr/bin/env python3
"""Emit immutable C/assembly resources from an explicit object palette plan.

No fitting or palette defaults here: source groups/words, CRAM pages, maps and
code/group assignments must all be supplied. Every output pixel is checked
against the lossless source atlas. The original source code remains its key.
"""
from pathlib import Path
import argparse,hashlib,json,re

def prepare(plan,source,raw):
 assert hashlib.sha256(raw).hexdigest()==source['binary_sha256']==plan['source_binary_sha256']
 groups=sorted(int(g) for g in plan['source_palettes'])
 palettes=[(g,plan['source_palettes'][str(g)]) for g in groups]
 assert 0<len(groups)<=32 and all(0<=g<32 for g in groups)
 assert all(len(w)==16 and all(0<=v<=65535 for v in w) for _,w in palettes)
 assert len(plan['cram'])==4 and all(len(p)==16 and all(v&~0xeee==0 for v in p) for p in plan['cram'])
 maps=[]
 for m in plan['maps']:
  assert m['source_group'] in groups and m['source_words']==dict(palettes)[m['source_group']]
  assert 0<=m['page']<4 and len(m['pens'])==16 and m['pens'][0]==0 and all(1<=p<=15 for p in m['pens'][1:])
  maps.append((groups.index(m['source_group']),m['page'],m['pens']))
 blocks=[bytes(128)];ids={blocks[0]:0};entries=[];previous=-1
 source_entries=dict(source['entries'])
 for e in plan['entries']:
  code,group,mi=e['code'],e['source_group'],e['map']
  key=(code<<5)|group
  assert key>previous;previous=key
  assert 0<=mi<len(maps) and groups[maps[mi][0]]==group
  assert source_entries[code]==e['source_block']
  chunk=raw[e['source_block']*128:(e['source_block']+1)*128];assert len(chunk)==128
  pens=maps[mi][2];chunk=bytes(pens[v>>4]<<4|pens[v&15] for v in chunk)
  if chunk not in ids:ids[chunk]=len(blocks);blocks.append(chunk)
  assert ids[chunk]==e['variant_block']
  entries.append((code,e['source_block'],ids[chunk],mi))
 blob=b''.join(blocks)
 assert len(blocks)==plan['blocks'] and hashlib.sha256(blob).hexdigest()==plan['binary_sha256']
 return palettes,maps,entries,blob

def emit(plan_path,source_prefix,output):
 plan=json.loads(plan_path.read_text());source=json.loads(source_prefix.with_suffix('.json').read_text())
 raw=source_prefix.with_suffix('.bin').read_bytes()
 palettes,maps,entries,blob=prepare(plan,source,raw)
 name=re.sub('[^a-zA-Z0-9_]','_',output.name);source_name=re.sub('[^a-zA-Z0-9_]','_',source_prefix.name)
 output.parent.mkdir(parents=True,exist_ok=True);output.with_suffix('.bin').write_bytes(blob)
 lines=['/* Explicit source-palette plan: validate and activate before use. */',f'#ifndef {name.upper()}_H',f'#define {name.upper()}_H','#include "willow_native_object_remap.h"',f'#include "{source_prefix.with_suffix(".h").resolve()}"',f'extern const uint8_t {name}_patterns[{len(blob)}];',f'static const WillowObjectTileAtlas {name}_variants={{0,{name}_patterns,{len(blob)}UL,0,{len(blob)//128}}};',f'static const uint16_t {name}_cram[64]={{'+','.join(hex(w) for page in plan['cram'] for w in page)+'};',f'static const WillowObjectRemapPalette {name}_palettes[]={{']
 lines+=['{{'+','.join(hex(w) for w in words)+'},'+str(group)+'},' for group,words in palettes]
 lines+=['};',f'static const WillowObjectRemapMap {name}_maps[]={{']
 lines+=['{'+str(group)+','+str(page)+',{'+','.join(str(p) for p in pens)+'}},' for group,page,pens in maps]
 lines+=['};',f'static const WillowObjectRemapEntry {name}_entries[]={{']
 lines+=['{'+','.join(str(v) for v in entry)+'},' for entry in entries]
 lines+=['};',f'static const WillowObjectRemapPlan {name}_plan={{',f'&{source_name}_atlas,&{name}_variants,{name}_entries,{name}_maps,',f'{name}_palettes,{name}_cram,{len(entries)},{len(maps)},{len(palettes)}}};','#endif']
 output.with_suffix('.h').write_text('\n'.join(lines)+'\n')
 output.with_suffix('.s').write_text(f'.section .rodata\n.balign 4\n.global {name}_patterns\n{name}_patterns:\n.incbin "{output.with_suffix(".bin").resolve()}"\n')
 return {'name':name,'entries':len(entries),'blocks':len(blob)//128,'pattern_bytes':len(blob),'metadata_bytes_68000':16+len(entries)*8+len(maps)*20+len(palettes)*34+128+30,'source_atlas_separate':True,'verified_pixels':len(entries)*256,'sha256':hashlib.sha256(blob).hexdigest()}

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--plan',type=Path,required=True);p.add_argument('--source-prefix',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 print(json.dumps(emit(a.plan,a.source_prefix,a.output),indent=2))
