"""Retain observed original ROM reads from the CPU-checked demo-start fixtures."""
from pathlib import Path
import hashlib
root=Path(__file__).resolve().parents[1]
program=(root/'build/reference_maincpu.bin').read_bytes()
access=(root/'build/demo-start-rom-access.bin').read_bytes()
if len(program)!=0x100000 or len(access)!=len(program) or any(x>1 for x in access):
    raise SystemExit('Expected complete program and Boolean observed-read footprint')
spans=[]
for at in range(0,len(access),2):
    if access[at]!=access[at+1]:raise SystemExit('Unaligned observed ROM read')
    if access[at]:
        if spans and spans[-1][1]==at:spans[-1][1]+=2
        else:spans.append([at,at+2])
if not spans:raise SystemExit('Empty footprint')
words=[];ranges=[]
for lo,hi in spans:
    ranges.append((lo,(hi-lo)//2,len(words)))
    words.extend(int.from_bytes(program[at:at+2],'big') for at in range(lo,hi,2))
if len(words)>65535:raise SystemExit('Offset width exceeded')
lines=['/* Original ROM reads observed in CPU-checked demo-start fixtures.',
       ' * This footprint is not proof of complete gameplay data coverage.',
       f' * Source SHA256: {hashlib.sha256(program).hexdigest()}',
       f' * Footprint SHA256: {hashlib.sha256(access).hexdigest()} */',
       '#ifndef WILLOW_NATIVE_DEMO_START_DATA_H','#define WILLOW_NATIVE_DEMO_START_DATA_H',
       'static const unsigned short nativeDemoStartWords[]={']
for n in range(0,len(words),12):lines.append(','.join(f'0x{x:04x}' for x in words[n:n+12])+',')
lines+=['};','static const struct {unsigned long first;unsigned short count,offset;} nativeDemoStartRanges[]={']
lines += [f'{{0x{lo:x}UL,{count},{offset}}},' for lo,count,offset in ranges]
lines += ['};','static inline unsigned char nativeDemoStartWordRead(unsigned long at,unsigned short*value){',
          'if(!value||(at&1))return 0;',f'unsigned short lo=0,hi={len(ranges)};',
          'while(lo<hi){unsigned short mid=(lo+hi)/2;',
          'unsigned long first=nativeDemoStartRanges[mid].first;',
          'if(at<first){hi=mid;continue;}',
          'unsigned long offset=(at-first)/2;',
          'if(offset<nativeDemoStartRanges[mid].count){',
          '*value=nativeDemoStartWords[nativeDemoStartRanges[mid].offset+offset];return 1;}',
          'lo=mid+1;}return 0;}','#endif']
(root/'native/intro_probe/src/demo_start_data.h').write_text('\n'.join(lines)+'\n')
print(f'Demo start: {len(words)*2} original bytes in {len(ranges)} observed ROM spans')
