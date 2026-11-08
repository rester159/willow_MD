#!/usr/bin/env python3
"""Pack arbitrary original CPS sprite codes into unflipped Genesis pen blocks.

--codes accepts a JSON integer list, or {"codes": [...]}; --opening-captures
selects codes from existing object banks without launching an emulator.
No color fitting, palette assignments, animation, or game logic are generated.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile, genesis_tiles, TILE_COUNT

ROOT=Path(__file__).resolve().parents[1]
def capture_codes(path):
 raw=path.read_bytes();codes=set();frames=[]
 if len(raw)<0x8800:raise ValueError(f'Short CPS capture: {path}')
 for bank in (0,0x8000):
  words=struct.unpack('>1024H',raw[bank:bank+0x800])
  for flip in (0,1):
   current=set();groups=set();pieces=0
   for at in range(0,1024,4):
    x,y,base,attr=words[at:at+4]
    if attr&0xff00==0xff00:break
    if base>0x27ff:continue
    nx=((attr>>8)&15)+1;ny=(attr>>12)+1
    for row in range(ny):
     for col in range(nx):
      sx=(x+col*16)&511;sy=(y+row*16)&511
      px=(496-sx if flip else sx)-96;py=(240-sy if flip else sy)-16
      if px>=320 or px+16<=0 or py>=224 or py+16<=0:continue
      cx=nx-1-col if attr&32 else col;cy=ny-1-row if attr&64 else row
      current.add((base&~15)+((base+cx)&15)+cy*16);groups.add(attr&31);pieces+=1
   codes.update(current)
   frames.append({'path':str(path),'bank':bank,'global_flip':flip,'visible_pieces_including_blank':pieces,'codes':sorted(current),'palette_groups':sorted(groups),'capture_sha256':hashlib.sha256(raw).hexdigest()})
 return codes,frames
def pack(gfx,codes):
 blocks=[bytes(128)];lookup={blocks[0]:0};entries=[]
 for code in sorted(set(codes)):
  if not 0<=code<TILE_COUNT:raise ValueError(f'Invalid source graphic code: {code}')
  original=decode_tile(gfx,code,16)
  row_major=genesis_tiles([[0 if pen==15 else pen+1 for pen in row] for row in original])
  raw=row_major[:32]+row_major[64:96]+row_major[32:64]+row_major[96:]
  if raw not in lookup:lookup[raw]=len(blocks);blocks.append(raw)
  entries.append((code,lookup[raw]))
 return entries,b''.join(blocks)
def verify(gfx,entries,blob):
 for code,block in entries:
  expected=decode_tile(gfx,code,16);raw=blob[block*128:(block+1)*128]
  if len(raw)!=128:raise ValueError('Short packed pattern')
  for y in range(16):
   for x in range(16):
    at=((x//8)*2+y//8)*32+(y&7)*4+(x&7)//2
    pen=(raw[at]>>(0 if x&1 else 4))&15
    source=15 if pen==0 else pen-1
    assert source==expected[y][x],(code,x,y,source,expected[y][x])
 return len(entries)*256
def generate(archive,codes,prefix,frames=()):
 if not codes:raise ValueError('At least one source code is required')
 with zipfile.ZipFile(archive) as z:gfx=assemble_region(z)
 entries,blob=pack(gfx,codes);pixels=verify(gfx,entries,blob)
 prefix.parent.mkdir(parents=True,exist_ok=True)
 name=re.sub('[^a-zA-Z0-9_]','_',prefix.name)
 if not name or name[0].isdigit():name='willow_'+name
 binary=prefix.with_suffix('.bin');header=prefix.with_suffix('.h');manifest=prefix.with_suffix('.json')
 binary.write_bytes(blob)
 lines=['/* Original source pens; no flips or RGB palette baked into this atlas. */',
  f'#ifndef {name.upper()}_H',f'#define {name.upper()}_H',
  '#include "willow_native_object_tiles.h"',
  f'#define {name.upper()}_COUNT {len(entries)}',f'#define {name.upper()}_BLOCKS {len(blob)//128}',
  f'extern const uint8_t {name}_patterns[{len(blob)}];',
  f'static const WillowObjectTileEntry {name}_entries[]={{']
 lines.extend('{0x%04x,%d},'%entry for entry in entries)
 lines.extend(['};',f'static const WillowObjectTileAtlas {name}_atlas={{',
  f'{name}_entries,{name}_patterns,{len(blob)}UL,{len(entries)},{len(blob)//128}'+'};','#endif'])
 header.write_text('\n'.join(lines)+'\n')
 # A directly assemblable immutable resource; SGDK integration can instead
 # import the same binary with a four-byte-aligned BIN resource declaration.
 assembly=prefix.with_suffix('.s')
 assembly.write_text('.section .rodata\n.balign 4\n'+f'.global {name}_patterns\n{name}_patterns:\n'+
  '.incbin '+json.dumps(str(binary.resolve()))+'\n'+f'.size {name}_patterns,.-{name}_patterns\n')
 e=dict(entries);frame_stats=[]
 for frame in frames:
  live={e[code] for code in frame['codes'] if e[code]}
  frame_stats.append(dict(frame,unique_nonblank_blocks=len(live),resident_8x8_tiles=len(live)*4,initial_dma_bytes=len(live)*128))
 report={'archive_sha256':hashlib.sha256(Path(archive).read_bytes()).hexdigest(),
  'binary_sha256':hashlib.sha256(blob).hexdigest(),'source_codes':len(entries),
  'unique_blocks_including_blank':len(blob)//128,'unique_nonblank_blocks':len(blob)//128-1,
  'pattern_bytes':len(blob),'entry_bytes_68000':len(entries)*4,'atlas_descriptor_bytes_68000':16,
  'resource_alignment':4,'assembly':str(assembly),
  'verified_pixels':pixels,'entries':entries,'captures':frame_stats,
  'peak_capture_unique_blocks':max((r['unique_nonblank_blocks'] for r in frame_stats),default=0),
  'peak_capture_resident_tiles':max((r['resident_8x8_tiles'] for r in frame_stats),default=0),
  'peak_capture_initial_dma_bytes':max((r['initial_dma_bytes'] for r in frame_stats),default=0),
  'pen_mapping':'CPS15->Genesis0; CPS0..14->Genesis1..15',
  'layout':'Unflipped16x16; four8x8 patterns TL,BL,TR,BR; deduplicated by all128bytes',
  'symbol':name,'scope':'Opening bank-capture code coverage when selected; not all-game coverage or a palette/priority/VRAM allocation. No actual cartridge admission.'}
 manifest.write_text(json.dumps(report,indent=2)+'\n')
 return report
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 source=parser.add_mutually_exclusive_group(required=True)
 source.add_argument('--codes',type=Path)
 source.add_argument('--opening-captures',action='store_true')
 parser.add_argument('--archive',type=Path,default=ROOT/'willow.zip')
 parser.add_argument('--output',type=Path,default=ROOT/'build/cartridge-gap-audit/opening_object_tiles')
 args=parser.parse_args();frames=[]
 if args.codes:
  values=json.loads(args.codes.read_text());codes=values.get('codes') if isinstance(values,dict) else values
  codes={int(value,0) if isinstance(value,str) else int(value) for value in codes}
 else:
  codes=set()
  captures=sorted((ROOT/'build/arcade-trace').glob('intro-vram-*.bin'))
  if not captures:raise ValueError('No existing opening captures')
  for path in captures:
   selected,metadata=capture_codes(path);codes.update(selected);frames.extend(metadata)
 report=generate(args.archive,codes,args.output,frames)
 print({key:value for key,value in report.items() if key not in ('entries','captures')})
if __name__=='__main__':main()
