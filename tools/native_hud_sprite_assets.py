#!/usr/bin/env python3
"""Plan source-derived HUD sprites without consuming a background plane.
The native renderer clones shared blocks before live mutations. Verify every source case.
"""
from pathlib import Path
import json
pixels=Path('build/native-hud-initial-tiles.bin').read_bytes()
assert len(pixels)==32*7680
rows=[0,1,24,26,27]
reports=[];default=None
for case in range(32):
 data=pixels[case*7680:(case+1)*7680]
 tile=lambda r,c:data[(r*40+c)*32:(r*40+c+1)*32]
 chunks=[]
 # Sparse single rows leave sprite pixel bandwidth for gameplay actors.
 for r in range(3):
  x=0
  while x<40:
   if not any(tile(r,x)):x+=1;continue
   width=1
   while width<4 and x+width<40 and any(tile(r,x+width)):width+=1
   chunks.append((x,rows[r],width,1,b''.join(tile(r,c) for c in range(x,x+width))))
   x+=width
 # Both bottom rows are completely opaque; 4x2 sprites share their patterns.
 assert all((v>>4) and (v&15) for v in data[120*32:200*32])
 for x in range(0,40,4):chunks.append((x,26,4,2,b''.join(tile(r,c) for c in range(x,x+4) for r in (3,4))))
 unique=list(dict.fromkeys(c[4] for c in chunks));sizes=[len(b)//32 for b in unique]
 # Original HUD/caption areas: FB00..FFFF, font[0..39], font[72..79],
 # and F700..F7FF. F680..F6FF remains the ninth Willow block.
 capacities=[40,40,8,8];assignment=[None]*len(unique)
 def place(todo,used):
  if not todo:return True
  i=todo[0];seen=set()
  for bank,cap in enumerate(capacities):
   remaining=cap-used[bank]
   if remaining in seen or sizes[i]>remaining:continue
   seen.add(remaining);assignment[i]=(bank,used[bank]);used[bank]+=sizes[i]
   if place(todo[1:],used):return True
   used[bank]-=sizes[i]
  return False
 fits=place(sorted(range(len(unique)),key=lambda i:-sizes[i]),[0]*4)
 # Reconstruct every tile, including omitted transparent cells.
 rebuilt=[bytes(32) for _ in range(200)];counts=[0]*224;widths=[0]*224
 for x,y,w,h,block in chunks:
  for line in range(y*8,(y+h)*8):counts[line]+=1;widths[line]+=w*8
  for col in range(w):
   for row in range(h):
    outrow=rows.index(y+row);at=(col*h+row)*32
    index=outrow*40+x+col
    assert rebuilt[index]==bytes(32)
    rebuilt[index]=block[at:at+32]
 assert b''.join(rebuilt)==data[:6400]
 assert len(chunks)<=80 and max(counts)<=20 and max(widths)<=320
 report={'case':case,'sprites':len(chunks),'unique_patterns':sum(sizes),'fits_existing_96_patterns':fits,
  'maximum_sprites_per_line':max(counts),'maximum_pixels_per_line':max(widths),
  'top_pixels_per_line':[widths[0],widths[8]],'source_pixel_checks':12800}
 reports.append(report)
 if case==10:
  assert fits
  banks=[bytearray(c*32) for c in capacities]
  for i,block in enumerate(unique):bank,offset=assignment[i];banks[bank][offset*32:offset*32+len(block)]=block
  packed=b''.join(banks);Path('native/intro_probe/res/hud_sprite_plan.bin').write_bytes(packed)
  default={'chunks':[{'x':x*8,'y':y*8,'width':w,'height':h,'bank':assignment[unique.index(block)][0],
   'offset':assignment[unique.index(block)][1]} for x,y,w,h,block in chunks], 'bank_patterns':capacities,
   'scope':'Cold Stage 0 preparation only. Runtime live writes update sprite layouts and clone shared pattern blocks.'}
Path('build/native-hud-sprite-plan.json').write_text(json.dumps({'cases':reports,'default':default},indent=2)+'\n')
print('HUD sprite layout:',reports[10]);print('All 32 source cases reconstructed exactly:',32*12800,'pixels')

header=['/* Generated ROM-derived cold HUD sprite layout. */',
 'typedef struct {u8 col,row,width,height;u16 pattern;} NativeHudSpritePlan;',
 f'#define NATIVE_HUD_SPRITE_COUNT {len(default["chunks"])}',
 'static const NativeHudSpritePlan native_hud_sprite_plan[]={']
starts=[0,40,80,88]
for c in default['chunks']:
 header.append('{'+','.join(map(str,[c['x']//8,c['y']//8,c['width'],c['height'],starts[c['bank']]+c['offset']]))+'},')
header+=['};']
Path('native/intro_probe/src/hud_sprite_plan.h').write_text('\n'.join(header)+'\n')
