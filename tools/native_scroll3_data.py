#!/usr/bin/env python3
"""Lossless Scroll3 dictionary, including the source A00 code adjustment."""
from pathlib import Path
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
records=[(((int.from_bytes(p[a:a+2],'big')+0xa00)&65535)<<16)|int.from_bytes(p[a+2:a+4],'big') for a in range(0xc0000,0xd0000,4)]
# 8402 MOVE.L writes one literal pair outside the chunk dictionary.
assert p[0x8402:0x8404]==bytes.fromhex('20bc')
backdrop_pair=int.from_bytes(p[0x8404:0x8408],'big')
pairs=sorted(set(records)|{0,backdrop_pair});assert len(pairs)<=2048
lookup={v:i for i,v in enumerate(pairs)}
lines=['/* Generated lossless original Scroll3 pairs; no captured state. */',
 '#ifndef WILLOW_NATIVE_SCROLL3_DATA_H','#define WILLOW_NATIVE_SCROLL3_DATA_H',
 f'#define WILLOW_SCROLL3_PAIR_COUNT {len(pairs)}',f'#define WILLOW_SCROLL3_BACKDROP_PAIR {lookup[backdrop_pair]}',f'#define WILLOW_SCROLL3_ZERO_PAIR {lookup[0]}',f'#define WILLOW_SCROLL3_CLEAR_PAIR {lookup[0x0a000000]}',
 'static const uint32_t nativeScroll3Pairs[]={']
lines.extend(','.join(f'0x{v:08x}UL' for v in pairs[n:n+8])+',' for n in range(0,len(pairs),8))
lines+=['};','#ifndef WILLOW_PACKED_SCROLL3_SOURCE_PROBE','static const uint16_t nativeScroll3SourceIds[]={']
ids=[lookup[v] for v in records]
lines.extend(','.join(map(str,ids[n:n+16]))+',' for n in range(0,len(ids),16))
packed=[]
for start in range(0,len(ids),16):
 value=0
 for i in ids[start:start+16]:value=(value<<11)|i
 packed.extend((value>>shift)&65535 for shift in range(160,-1,-16))
lines+=['};','#endif','static const uint16_t nativeScroll3PackedSourceIds[]={']
lines.extend(','.join(map(str,packed[n:n+16]))+',' for n in range(0,len(packed),16))
lines+=['};']
spans=[(0x2eb24,0x2eb4b),(0x42fc4,0x42fcb),(0x54f2,0x5532),(0x43d3e,0x43d4e)]
for stage in range(7):
 at=0x2eb24+stage*4;layout=int.from_bytes(p[at:at+4],'big');mode=p[0x2eb44+stage]
 spans.append((layout,layout+(256,256,2048,512)[mode]+96))
# Extra attract layout selected by 83C2 after scene reset (mode zero).
layout=int.from_bytes(p[0x2eb40:0x2eb44],'big')
spans.append((layout,layout+256))
merged=[]
for start,end in sorted(spans):
 if merged and start<=merged[-1][1]:merged[-1]=(merged[-1][0],max(end,merged[-1][1]))
 else:merged.append((start,end))
for n,(start,end) in enumerate(merged):
 lines.append(f'static const uint8_t scroll3_data_{n}[]={{')
 lines.extend(','.join(str(v) for v in p[a:min(a+16,end)])+',' for a in range(start,end,16))
 lines.append('};')
lines.append('static inline uint8_t nativeScroll3ByteRead(uint32_t at,uint8_t*v){')
for n,(start,end) in enumerate(merged):
 lines.append(f'if(at>=0x{start:x}UL && at<0x{end:x}UL){{*v=scroll3_data_{n}[at-0x{start:x}UL];return 1;}}')
lines+=['return 0;}']
wordspans=[(0x43d3e,0x43d4e),(0x54f2,0x5532),(0x2eb24,0x2eb44)]
for n,(start,end) in enumerate(wordspans):
 lines.append(f'static const uint16_t scroll3_words_{n}[]={{'+','.join(str(int.from_bytes(p[a:a+2],'big')) for a in range(start,end,2))+'};')
lines.append('static inline uint8_t nativeScroll3WordRead(uint32_t at,uint16_t*v){')
lines.append('if(at&1)return 0;')
for n,(start,end) in enumerate(wordspans):
 lines.append(f'if(at>=0x{start:x}UL && at<0x{end:x}UL){{*v=scroll3_words_{n}[(at-0x{start:x}UL)>>1];return 1;}}')
lines+=['return 0;}','#endif']
Path('native/intro_probe/src/scroll3_data.h').write_text('\n'.join(lines)+'\n')
print(f'Scroll3: {len(records)} source cells, {len(pairs)} exact pairs, 5632 map RAM bytes')
