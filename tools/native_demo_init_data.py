"""Retain original reset/difficulty/checkpoint words required by demo entry."""
from pathlib import Path
import hashlib
root = Path(__file__).resolve().parents[1]
program = (root/'build/reference_maincpu.bin').read_bytes()
if len(program) != 0x100000:
    raise SystemExit('Expected complete original program')
spans = [(0,2),(0x183c,0x1844),(0x2dbe,0x2de4),(0x2f72,0x2f7a),
         (0x2fa2,0x2fba),(0x304c,0x3050),(0x3058,0x3078),
         (0x3106,0x3114),(0x3819a,0x381b0)]
for stage,count in ((0,7),(1,10)):
    at = 0x304c + int.from_bytes(program[0x304c+stage*2:0x304e+stage*2], 'big', signed=True)
    spans.append((at,at+count*8))
merged=[]
for lo,hi in sorted(spans):
    assert not (lo&1 or hi&1)
    if merged and lo<=merged[-1][1]:
        merged[-1]=(merged[-1][0],max(hi,merged[-1][1]))
    else: merged.append((lo,hi))
lines=['/* Original demo-entry player reset/difficulty/checkpoint data.',
       f' * Source SHA256: {hashlib.sha256(program).hexdigest()} */',
       '#ifndef WILLOW_NATIVE_DEMO_INIT_DATA_H','#define WILLOW_NATIVE_DEMO_INIT_DATA_H']
for n,(lo,hi) in enumerate(merged):
    words=[int.from_bytes(program[a:a+2],'big') for a in range(lo,hi,2)]
    lines += [f'static const unsigned short nativeDemoInitWords{n}[]={{']
    lines += [','.join(f'0x{x:04x}' for x in words[i:i+12])+',' for i in range(0,len(words),12)]
    lines += ['};']
lines += ['static inline unsigned char nativeDemoInitWordRead(unsigned long at,unsigned short*value){',
          'if(!value||(at&1))return 0;']
for n,(lo,hi) in enumerate(merged):
    lines += [f'if(at>=0x{lo:x}UL&&at<0x{hi:x}UL){{*value=nativeDemoInitWords{n}[(at-0x{lo:x}UL)/2];return 1;}}']
lines += ['return 0;}','#endif']
(root/'native/intro_probe/src/demo_init_data.h').write_text('\n'.join(lines)+'\n')
print(f'Demo init: {sum(hi-lo for lo,hi in merged)} original bytes in {len(merged)} spans')
