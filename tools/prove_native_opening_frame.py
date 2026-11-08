import sys,json,zipfile,struct
from pathlib import Path
sys.path.insert(0,'tools')
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb,palette_words
from cps1_source_layers import SourceLayers
from cps1_intro_actor_render import actor_trace,emit
from native_scroll1_palette_plan import source_cell
root=Path('build/arcade-trace');frame=globals().get('FRAME',1020);p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
layers=SourceLayers(p,g);ram=(root/f'intro-vram-{frame}.bin').read_bytes();actual=Image.open(root/f'natural-intro-{frame}.png').convert('RGB').crop((32,0,352,224));out=Image.new('RGB',(320,224));pal={layer:[[cps_rgb(w|0xf000) for w in palette_words(p,f'scroll{layer}',0,group)] for group in range(32)] for layer in (2,3)}
for y in range(224):
 for x in range(320):
  for layer in (2,3):
   pen,group=layers.pen(layer,x+32,y-240)
   if pen!=15:out.putpixel((x,y),pal[layer][group][pen]);break
actors={(a['frame'],a['slot']):a for a in actor_trace()};covered=set();records=[];cache={}
for slot in (1,2,0):
 if 'SOURCE_BANK' in globals():
  at=SOURCE_BANK+(1,2,0).index(slot)*8;x,y,code,attribute=struct.unpack('>4H',ram[at:at+8]);r={'x':x,'y':y,'code':code,'attribute':attribute,'width':((attribute>>8)&15)+1,'height':((attribute>>12)&15)+1}
 else:
  a=actors[frame-2,slot];r=emit(p,a,0,a['camera'])
 records.append([r[k] for k in ('x','y','code','attribute')])
 for cy in range(r['height']):
  for cx in range(r['width']):
   code=(r['code']&~15)+((r['code']+cx)&15)+cy*16
   if code not in cache:cache[code]=decode_tile(g,code,16)
   for yy in range(16):
    for xx in range(16):
     pen=cache[code][yy][xx]
     if pen==15:continue
     x=cx*16+xx;y=cy*16+yy
     if r['attribute']&32:x=r['width']*16-1-x
     if r['attribute']&64:y=r['height']*16-1-y
     x+=r['x']-96;y+=r['y']-16
     if not(0<=x<320 and 0<=y<224) or (x,y) in covered:continue
     covered.add((x,y));at=0x1c000+(r['attribute']&31)*32+pen*2;out.putpixel((x,y),cps_rgb(int.from_bytes(ram[at:at+2],'big')))
for row in range(2,30):
 for col in range(4,44):
  code,attr=source_cell(ram,col,row,0x10000)
  if not 0x7000<=code<=0x7fff:continue
  key=code,col&1
  if key not in cache:cache[key]=decode_tile(g,code,8,col&1)
  for y in range(8):
   for x in range(8):
    pen=cache[key][7-y if attr&64 else y][7-x if attr&32 else x]
    if pen!=15:
     at=0x1c400+(attr&31)*32+pen*2;out.putpixel(((col-4)*8+x,(row-2)*8+y),cps_rgb(int.from_bytes(ram[at:at+2],'big')))
bad=[(x,y,out.getpixel((x,y)),actual.getpixel((x,y))) for y in range(224) for x in range(320) if out.getpixel((x,y))!=actual.getpixel((x,y))]
print('mismatches',len(bad),bad[:20]);print('objects',records)
out.save(f'build/cartridge-gap-audit/source-composition-{frame}.png')
Path(f'build/cartridge-gap-audit/source-composition-{frame}.json').write_text(json.dumps({'frame':frame,'source_actor_frame':None if 'SOURCE_BANK' in globals() else frame-2,'source_object_bank':globals().get('SOURCE_BANK'),'pixels':320*224,'mismatches':len(bad),'first_mismatches':bad[:20],'object_records':records},indent=2)+'\n')
assert not bad
