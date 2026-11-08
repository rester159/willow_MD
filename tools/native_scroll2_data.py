#!/usr/bin/env python3
"""Lossless dictionary for all original Scroll2 code/attribute records."""
from pathlib import Path
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
records=[int.from_bytes(p[a:a+4],'big') for a in range(0x80000,0xc0000,4)]
pairs=sorted(set(records)|{0})
assert len(pairs)<32768
lookup={value:i for i,value in enumerate(pairs)}
lines=['/* Generated original tile records; no quantization or captured state. */',
       '#ifndef WILLOW_NATIVE_SCROLL2_DATA_H','#define WILLOW_NATIVE_SCROLL2_DATA_H',
       f'#define WILLOW_SCROLL2_PAIR_COUNT {len(pairs)}',
       f'#define WILLOW_SCROLL2_ZERO_PAIR {lookup[0]}',
       'static const uint32_t nativeScroll2Pairs[]={']
lines.extend(','.join(f'0x{v:08x}UL' for v in pairs[n:n+8])+',' for n in range(0,len(pairs),8))
lines.extend(['};','static const uint16_t nativeScroll2SourceIds[]={'])
ids=[lookup[value] for value in records]
lines.extend(','.join(map(str,ids[n:n+16]))+',' for n in range(0,len(ids),16))
lines.extend(['};','#endif'])
Path('native/intro_probe/src/scroll2_data.h').write_text('\n'.join(lines)+'\n')
print(f'Scroll2: {len(records)} source records, {len(pairs)} exact dictionary pairs')
