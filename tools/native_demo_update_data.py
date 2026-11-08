"""Retain observed original ROM reads from the CPU-checked first demo update."""
from pathlib import Path
import hashlib
import argparse
parser=argparse.ArgumentParser()
parser.add_argument("--sequence", action="store_true")
parser.add_argument("--input-prefix", type=Path)
parser.add_argument("--output", type=Path)
parser.add_argument("--updates", type=int, default=120)
parser.add_argument("--pack-words", action="store_true")
args=parser.parse_args()
label=f"{args.updates}-update demo service sequence" if args.sequence else "first demo update"
name="demo-sequence" if args.sequence else "demo-update"
root=Path(__file__).resolve().parents[1]
program=(root/'build/reference_maincpu.bin').read_bytes()
access=Path(str(args.input_prefix)+'-rom-access.bin').read_bytes() if args.input_prefix else (root/f'build/{name}-rom-access.bin').read_bytes()
if len(program)!=0x100000 or len(access)!=len(program) or any(x>1 for x in access):
    raise SystemExit('Expected complete program and Boolean observed-read footprint')
spans=[]
for at in range(0,len(access),2):
    # Byte reads retain their containing word without truncating precision.
    if access[at] or access[at+1]:
        if spans and spans[-1][1]==at:spans[-1][1]+=2
        else:spans.append([at,at+2])
if not spans:raise SystemExit('Empty footprint')
words=[];ranges=[]
for lo,hi in spans:
    ranges.append((lo,(hi-lo)//2,len(words)))
    words.extend(int.from_bytes(program[at:at+2],'big') for at in range(lo,hi,2))
if len(words)>65535:raise SystemExit('Offset width exceeded')
lines=[f'/* Original ROM reads observed in CPU-checked {label}.',
       ' * This footprint is not proof of complete gameplay data coverage.',
       f' * Source SHA256: {hashlib.sha256(program).hexdigest()}',
       f' * Footprint SHA256: {hashlib.sha256(access).hexdigest()} */',
       '#ifndef WILLOW_NATIVE_DEMO_UPDATE_DATA_H','#define WILLOW_NATIVE_DEMO_UPDATE_DATA_H']
if args.pack_words:
    packed=[];at=0
    def run_length(at):
        end=at+1
        while end<len(words) and words[end]==words[at] and end-at<32767:end+=1
        return end-at
    while at<len(words):
        run=run_length(at)
        if run>=3:
            packed.extend([0x8000|run,words[at]]);at+=run
        else:
            first=at;at+=run
            while at<len(words) and at-first<32765:
                run=run_length(at)
                if run>=3:break
                at+=run
            packed.extend([at-first]+words[first:at])
    decoded=[];at=0
    while at<len(packed):
        tag=packed[at];at+=1;count=tag&32767
        if tag&32768:decoded.extend([packed[at]]*count);at+=1
        else:decoded.extend(packed[at:at+count]);at+=count
    assert decoded==words
    lines += [f'static unsigned short nativeDemoUpdateWords[{len(words)}];',
              'static void nativeDemoUpdateDataInit(void){',
              'static const unsigned short packed[]={']
    for n in range(0,len(packed),12):lines.append(','.join(f'0x{x:04x}' for x in packed[n:n+12])+',')
    lines += ['};','const unsigned short*in=packed;unsigned short*out=nativeDemoUpdateWords;',
              'while(in<packed+sizeof(packed)/sizeof(packed[0])){',
              'unsigned short tag=*in++,count=tag&32767;',
              'if(tag&32768){unsigned short value=*in++;while(count--)*out++=value;}',
              'else while(count--)*out++=*in++;','}','}']
else:
    lines += ['static const unsigned short nativeDemoUpdateWords[]={']
    for n in range(0,len(words),12):lines.append(','.join(f'0x{x:04x}' for x in words[n:n+12])+',')
    lines += ['};']
lines += ['static const struct {unsigned long first;unsigned short count,offset;} nativeDemoUpdateRanges[]={']
lines += [f'{{0x{lo:x}UL,{count},{offset}}},' for lo,count,offset in ranges]
lines += ['};']
# Restrict each binary search to spans intersecting this 4 KiB source page.
# Bounds never widen a span: the existing exact offset/count test still owns reads.
pages=[]
for page in range(256):
    lo=next((i for i,(start,count,_) in enumerate(ranges) if start+count*2>page*4096),len(ranges))
    hi=next((i for i,(start,_,_) in enumerate(ranges) if start>=(page+1)*4096),len(ranges))
    pages.append((lo,hi))
index_type='unsigned char' if len(ranges)<=255 else 'unsigned short'
lines += [f'static const {index_type} nativeDemoUpdatePages[256][2]={{']
for n in range(0,256,8):lines.append(','.join('{%d,%d}'%p for p in pages[n:n+8])+',')
lines += ['};','static inline unsigned char nativeDemoUpdateWordRead(unsigned long at,unsigned short*value){',
          'if(!value||(at&1)||at>=0x100000UL)return 0;']
if args.sequence:
    lines += ["#ifndef WILLOW_DEMO_ROM_LEAF_PROBE"]
    # CPU argument tracing: this retained original span serves 44.8% of reads.
    # Selecting a fast route changes neither coverage nor original word values.
    hot=next((r for r in ranges if r[0]==0x3c7b6),None)
    if hot is None:raise SystemExit('Profiled ROM span changed; remeasure before regenerating')
    first,count,offset=hot
    lines += [f'unsigned long hotOffset=at-0x{first:x}UL;',
              f'if(hotOffset<{count*2}UL){{*value=nativeDemoUpdateWords[{offset}+(unsigned short)(hotOffset/2)];return 1;}}']
if args.sequence:
    # Profiled original-data tables; choose their complete retained spans rather
    # than inventing coverage. All holes still use the exact fallback lookup.
    anchors=[0x27320,0xea260,0x36cb4,0x3b9b2]
    lines += ['#ifdef WILLOW_DEMO_ROM_DIRECT_SPANS_PROBE']
    selected=set()
    for anchor in anchors:
        span=next((r for r in ranges if r[0]<=anchor<r[0]+r[1]*2),None)
        if span is None:raise SystemExit(f'Missing profiled original span at {anchor:x}')
        if span in selected:continue
        selected.add(span)
        first,count,offset=span
        lines += [f'unsigned long direct{first:x}=at-0x{first:x}UL;',
                  f'if(direct{first:x}<{count*2}UL){{*value=nativeDemoUpdateWords[{offset}+(unsigned short)(direct{first:x}/2)];return 1;}}']
    lines += ['#endif', '#endif']
lines += [
          'unsigned short page=at>>12,lo=nativeDemoUpdatePages[page][0],hi=nativeDemoUpdatePages[page][1];',
          'while(lo<hi){unsigned short mid=(lo+hi)/2;',
          'unsigned long first=nativeDemoUpdateRanges[mid].first;',
          'if(at<first){hi=mid;continue;}',
          'unsigned long offset=(at-first)/2;',
          'if(offset<nativeDemoUpdateRanges[mid].count){',
          '*value=nativeDemoUpdateWords[nativeDemoUpdateRanges[mid].offset+offset];return 1;}',
          'lo=mid+1;}return 0;}']
if args.sequence:
    # A native 68000 leaf uses only caller-saved D0/D1/A0. Tail fallback retains
    # the original caller stack, source-RAM behavior, and fault reporting.
    leaf=[hot]+[next(r for r in ranges if r[0]<=a<r[0]+r[1]*2) for a in anchors]
    assembly=[]
    for i,(first,count,offset) in enumerate(leaf,1):
        assembly += [f'move.l %d1,%d0',f'sub.l #0x{first:x},%d0',
                     f'cmp.l #{count*2},%d0',f'bhs {i}f',
                     f'lea nativeDemoSequenceWords+{offset*2},%a0',
                     'move.w (%a0,%d0.l),%d0','rts',f'{i}:']
    import json
    lines += ['#define NATIVE_DEMO_SEQUENCE_LEAF_ASM '+json.dumps('\n'.join(assembly)+'\n')]
lines += ['#endif']
output='\n'.join(lines)+'\n'
if args.sequence:output=output.replace('nativeDemoUpdate','nativeDemoSequence').replace('NATIVE_DEMO_UPDATE','NATIVE_DEMO_SEQUENCE')
header=name.replace('-', '_')+'_data.h'
(args.output or (root/'native/intro_probe/src'/header)).write_text(output)
print(f'{label}: {len(words)*2} original bytes in {len(ranges)} observed ROM spans')

if args.pack_words:print(f'Packed words: {len(packed)*2} ROM bytes expand to {len(words)*2} RAM bytes')
