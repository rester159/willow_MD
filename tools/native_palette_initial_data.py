#!/usr/bin/env python3
"""Extract source-addressed boot, stage and attract palette initialization data."""
from pathlib import Path
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
spans=[(0xa12,0xa36),(0xa52,0xa92),(0xe5080,0xe5280),(0xe6ea0,0xe72a0)]
# 076C/08FE reset copies six complete palette banks.
spans.extend((base,base+1024) for base in [0xe5080,0xe6ea0,0xe7d20,0xeab00,0xe6080,0xe64a0])
# 0AC4 attract initialization (two variants); merge overlaps with stage data.
spans.extend([(0xe9920,0xea120),(0xeb700,0xebb00),
              (0xe72a0,0xe76a0),(0xe5280,0xe5480)])
for stage in range(7):
    spans.extend([(0xe5480+stage*512,0xe5680+stage*512),
                  (0xe7d20+stage*1024,0xe8120+stage*1024)])
    at=0xa12+stage*4
    source=int.from_bytes(p[at:at+4],'big')
    spans.append((source,source+512))
# Opening controller sequences 0/1/2/3/4 and every palette frame they reference.
# Retain original selector offsets and terminator bytes, not guessed timings.
spans.extend([(0xc16,0xc1e),(0xd26,0xd2e),(0xd80,0xd8a)])
for sequence,sources in [(0,[0xea660]),(1,[0xea700]),(2,[0xebb00,0xebb60]),(3,[0xebbc0,0xebca0,0xebd80]),(4,[0xea620])]:
    at=0xd80+int.from_bytes(p[0xd80+sequence*2:0xd82+sequence*2],'big',signed=True)
    end=at;frames=[]
    while p[end]<128:
        frames.append(p[end]);end+=1
    assert p[end] in (254,255) and frames
    spans.append((at&~1,(end+2)&~1))
    for source in sources:spans.append((source,source+(max(frames)+1)*32))
# 898A type-zero panel controller cycles through frames zero through nine.
spans.append((0xe7aa0,0xe7aa0+10*32))
merged=[]
for start,end in sorted(spans):
    assert not(start&1 or end&1)
    if merged and start<=merged[-1][1]:merged[-1]=(merged[-1][0],max(end,merged[-1][1]))
    else:merged.append((start,end))
lines=['/* Generated original boot/stage/attract palette words; all brightness/RGB bits retained. */',
       '#ifndef WILLOW_NATIVE_PALETTE_INITIAL_DATA_H','#define WILLOW_NATIVE_PALETTE_INITIAL_DATA_H']
for n,(start,end) in enumerate(merged):
    values=[int.from_bytes(p[a:a+2],'big') for a in range(start,end,2)]
    lines.append(f'static const unsigned short palette_initial_{n}[]={{')
    lines.extend(','.join(f'0x{v:04x}' for v in values[i:i+12])+',' for i in range(0,len(values),12))
    lines.append('};')
lines.extend(['static inline unsigned char nativePaletteInitialWordRead(unsigned long at,unsigned short *value){','if(at&1)return 0;'])
for n,(start,end) in enumerate(merged):
    lines.append(f'if(at>=0x{start:x}UL && at<0x{end:x}UL){{*value=palette_initial_{n}[(at-0x{start:x}UL)/2];return 1;}}')
lines.extend(['return 0;}','#endif'])
Path('native/intro_probe/src/palette_initial_data.h').write_text('\n'.join(lines)+'\n')
print(f'Native initial palettes: {sum(end-start for start,end in merged)} original bytes in {len(merged)} spans')
