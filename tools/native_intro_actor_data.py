#!/usr/bin/env python3
"""Extract original animation, position, panel and stage-entry ROM records."""
from pathlib import Path
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
segments=[('spawn',0x4034a,0x4063a),('actor',0x325bc,0x32ae6),('player',0xd07e,0xd166),
          ('panel',0x7c50,0x7d70),('stage',0x9ad8,0x9ef2),
          ('palette_entry',0x410ec,0x41246)]
# Follow the title animation's source frame chain until its explicit loop.
w=lambda a:int.from_bytes(p[a:a+2],'big')
a=0x15510+w(0x15510)
seen=set();title_start=0x15510;title_end=title_start+2
while a not in seen:
    assert 0<=a<len(p)-10 and not a&1
    seen.add(a)
    count=w(a);size=(count&0x7fff)*(4 if count&0x8000 else 2)
    if w(a+2)&0xff00:size=2
    end=a+10+size;jump=w(a+4)&0x8000
    title_start=min(title_start,a);title_end=max(title_end,end+(4 if jump else 0))
    a=int.from_bytes(p[end:end+4],'big') if jump else end
segments.append(('title',title_start,title_end))
# Retain precisely the geometry selectors and coordinate pairs consumed by
# the ordinary sprite writer for these original frames.
geometry_spans=set()
for frame in seen:
    selector=w(frame+4);pointer=0x3b9b2+((selector&0x7f80)>>5)
    table=int.from_bytes(p[pointer:pointer+4],'big')
    entry=table+(selector&127)*2
    geometry=table+int.from_bytes(p[entry:entry+2],'big',signed=True)
    pieces=w(frame)&0x7fff
    if not w(frame)&0x8000 and w(frame+2)&0xff00:pieces=1
    geometry_spans.update([(pointer,pointer+4),(entry,entry+2),(geometry,geometry+pieces*4)])
for n,(start,end) in enumerate(sorted(geometry_spans)):
    segments.append((f'title_geometry_{n}',start,end))

lines=['/* Original ROM records; no captured runtime state. */']
for name,start,end in segments:
 words=[int.from_bytes(p[i:i+2],'big') for i in range(start,end,2)]
 lines.append('static const unsigned short intro_'+name+'_words[]={'+','.join(map(str,words))+'};')
lines.append('static unsigned short introActorWord(unsigned long address){')
for address in (0x3078,0x307a):
 lines.append(f'if(address==0x{address:x}UL)return {int.from_bytes(p[address:address+2],"big")};')
for name,start,end in segments:
 lines.append(f'if(address>=0x{start:x}UL && address<0x{end:x}UL)return intro_{name}_words[(address-0x{start:x}UL)/2];')
# Cold-start-only lookups stay after hot animation/stage-entry ranges.
for address in (0x1bf2,0x2f72):
 lines.append(f'if(address==0x{address:x}UL)return {int.from_bytes(p[address:address+2],"big")};')
lines.append('return 0xffff;}')
lines.append('static inline unsigned char introTitleFrameSupported(unsigned long address){return '+ ' || '.join(f'address==0x{a:x}UL' for a in sorted(seen))+';}')
Path('native/intro_probe/src/actor_data.h').write_text('\n'.join(lines)+'\n')
