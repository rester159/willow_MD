#!/usr/bin/env python3
"""Pack source-addressed Scroll2 controller tables for the native runtime."""
from pathlib import Path
from cps1_program import assemble_maincpu

program = assemble_maincpu(Path('willow.zip'))
# Exact addressable spans: stage descriptors, each layout selector domain,
# cold-load offsets, exposure offsets, enable flags, and bound lookup domains.
spans = [(0x2dbf8, 0x2dc23), (0x54f2, 0x5532), (0x43504, 0x43514),
         (0x42712, 0x42719), (0x42e82, 0x42e90)]
for stage in range(7):
    at = 0x2dbf8 + stage * 4
    layout = int.from_bytes(program[at:at+4], 'big')
    mode = program[0x2dc1c + stage]
    spans.append((layout, layout + (256, 256, 2048, 512)[mode]))
    at = 0x42e82 + stage * 2
    bounds = 0x42e82 + int.from_bytes(program[at:at+2], 'big', signed=True)
    spans.append((bounds, bounds + 256))
# 82BE reads the additional descriptor at 2DC14. Scene reset leaves mode
# zero, whose complete selector domain is 256 bytes (including later pans).
attract_layout = int.from_bytes(program[0x2dc14:0x2dc18], 'big')
spans.append((attract_layout, attract_layout + 256))
merged = []
for start, end in sorted(spans):
    if merged and start <= merged[-1][1]:
        merged[-1] = (merged[-1][0], max(end, merged[-1][1]))
    else:
        merged.append((start, end))
lines = ['/* Generated source data only; no captured state or executed arcade code. */',
         '#ifndef WILLOW_NATIVE_CAMERA_DATA_H', '#define WILLOW_NATIVE_CAMERA_DATA_H',
         f'#define WILLOW_CAMERA_DATA_BYTES {sum(end-start for start,end in merged)}']
for n, (start, end) in enumerate(merged):
    values = program[start:end]
    lines.append(f'static const unsigned char camera_data_{n}[]={{')
    lines.extend(','.join(f'0x{v:02x}' for v in values[i:i+16])+',' for i in range(0,len(values),16))
    lines.append('};')
lines.append('static inline unsigned char nativeCameraByteRead(unsigned long address,unsigned char *value){')
for n, (start, end) in enumerate(merged):
    lines.append(f'if(address>=0x{start:x}UL && address<0x{end:x}UL){{*value=camera_data_{n}[address-0x{start:x}UL];return 1;}}')
lines.append('return 0;}')
for n,(start,end) in enumerate(merged):
 start=(start+1)&~1
 lines.append(f'static const unsigned short camera_words_{n}[]={{')
 values=[int.from_bytes(program[a:a+2],'big') for a in range(start,end-1,2)]
 lines.extend(','.join(str(v) for v in values[i:i+16])+',' for i in range(0,len(values),16))
 lines.append('};')
lines.append('static inline unsigned char nativeCameraWordRead(unsigned long address,unsigned short *value){')
lines.append('if(address&1)return 0;')
for n,(start,end) in enumerate(merged):
 start=(start+1)&~1
 lines.append(f'if(address>=0x{start:x}UL && address<0x{end-1:x}UL){{*value=camera_words_{n}[(address-0x{start:x}UL)>>1];return 1;}}')
lines.extend(['return 0;}','#endif'])
Path('native/intro_probe/src/camera_data.h').write_text('\n'.join(lines)+'\n')
print(f'Native camera data: {sum(end-start for start,end in merged)} bytes in {len(merged)} ROM spans')
