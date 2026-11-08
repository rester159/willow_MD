#!/usr/bin/env python3
"""Preconvert immutable near source-pair IDs to native two-row column records."""
from pathlib import Path
import argparse,hashlib,json,re,struct
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(catalog,out,base=0x142000):
 out.mkdir(parents=True,exist_ok=True)
 manifest=json.loads((catalog/'manifest.json').read_text());layout=json.loads((catalog/'layout.json').read_text())
 for name in ['catalog.bin','catalog_imports.h','layout.json']:assert sha(catalog/name)==manifest['outputs'][name],('Catalog changed',name)
 h=(catalog/'catalog_imports.h').read_text();defs={k:int(v.rstrip('UL'),0)for k,v in re.findall(r'^#define\s+(\w+)\s+(0x[0-9a-fA-F]+[UL]*|[0-9]+[UL]*)$',h,re.M)}
 raw=(catalog/'catalog.bin').read_bytes();origin=layout['origin'];assert origin==defs['WILLOW_BG_CATALOG_ROM_BASE'] and len(raw)==layout['total_bytes'];assert struct.unpack_from('>IHH',raw)==(0x57424331,1,2);
 n=defs['WILLOW_BG_CATALOG_LAYER2_PAIR_COUNT'];refs=defs['WILLOW_BG_CATALOG_LAYER2_REFS_ADDRESS']-origin;flags=defs['WILLOW_BG_CATALOG_LAYER2_FLAGS_ADDRESS']-origin
 assert 0<n<=1023 and base%2==0
 data=bytearray()
 for ident in range(n):
  for col in range(2):
   top=ident*4+col;bottom=top+2
   words=[struct.unpack_from('>H',raw,refs+ref*4+2)[0]for ref in [top,bottom]]
   opaque=[int(raw[flags+ref]!=1)for ref in [top,bottom]]
   data+=struct.pack('>HHBBH',*words,opaque[0]|opaque[1]<<1,0,opaque[0]<<8|opaque[1])
 for ident in range(n):
  for col in range(2):
   words,bits,packed=struct.unpack_from('>IBxH',data,ident*16+col*8)
   a,b=ident*4+col,ident*4+col+2
   assert words>>16==struct.unpack_from('>H',raw,refs+a*4+2)[0] and words&65535==struct.unpack_from('>H',raw,refs+b*4+2)[0]
   assert bits==int(raw[flags+a]!=1)|(int(raw[flags+b]!=1)<<1)
   assert packed==(int(raw[flags+a]!=1)<<8)|int(raw[flags+b]!=1)
 (out/'near-columns.bin').write_bytes(data)
 (out/'near_columns_imports.h').write_text(f'#define WILLOW_BG_NEAR_COLUMNS_ADDRESS {base:#x}UL\n#define WILLOW_BG_NEAR_COLUMNS_COUNT {n}U\n#define WILLOW_BG_NEAR_COLUMNS_BYTES {len(data)}U\n#define WILLOW_BG_NEAR_COLUMNS_STRIDE 16U\n')
 manifest={'schema_version':1,'scope':__doc__,'address':base,'count':n,'bytes':len(data),'pair_stride':16,'column_stride':8,'record':'big-endian top word,bottom word,2-bit nonopaque flags,reserved0,packed top/bottom nonopaque bytes','original_full_attribute_identity':'unchanged catalog ID; no key/attribute remap','inputs':{str(p):sha(p)for p in [catalog/'catalog_imports.h',catalog/'catalog.bin',catalog/'layout.json',catalog/'manifest.json',Path(__file__)]},'outputs':{name:sha(out/name)for name in ['near-columns.bin','near_columns_imports.h']}}
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--catalog',type=Path,default=ROOT/'build/native-background-catalog/prepared-v1');p.add_argument('--output',type=Path,required=True);p.add_argument('--base',type=lambda x:int(x,0),default=0x142000);a=p.parse_args();print(json.dumps(build(a.catalog.resolve(),a.output.resolve(),a.base),indent=2))
