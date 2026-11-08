#!/usr/bin/env python3
"""Compare source-derived object colors in already captured arcade geometry.

Original PNGs remain untouched. Derived context columns replace only opaque
object pixels that exactly match the source rendering; arcade backgrounds and
text remain unchanged, so these are object-color diagnostics, not native frames.
"""
from pathlib import Path
import argparse,csv,hashlib,json,zipfile
from PIL import Image,ImageDraw
from cps1_gfx import assemble_region,decode_tile
from cps1_palette import cps_rgb
from cps1_program import assemble_maincpu
from cps1_intro_actor_render import actor_trace,emit,word
from native_palette_fit import color_error

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit';TRACE=ROOT/'build/arcade-trace'
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--candidates',nargs=2,default=['joint_weight_1','joint_weight_16']);parser.add_argument('--prefix',default='object-capture');args=parser.parse_args()
def rgb(w):return tuple(((w>>s)&7)*34 for s in (1,5,9))
program=assemble_maincpu(ROOT/'willow.zip')
with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
actors={(r['frame'],r['slot']):r for r in actor_trace()}
plans=[json.loads((OUT/f'object_palette_{name}.json').read_text()) for name in args.candidates]
lookups=[{(e['code'],e['source_group']):p['maps'][e['map']] for e in p['entries']} for p in plans]
oldnpc=[rgb(w) for w in json.loads((ROOT/'build/intro-sprite-assets.json').read_text())['palette'][1:]]
oldplayer=[rgb(w) for w in json.loads((ROOT/'build/intro-font.json').read_text())['genesis_palette'][1:]]
cache={};evidence=[]
for frame in (990,1020,1080):
 capture_path=TRACE/f'natural-intro-{frame}.png';capture=Image.open(capture_path).convert('RGB');original=capture.crop((32,0,352,224))
 ram=(TRACE/f'intro-vram-{frame}.bin').read_bytes();plane={}
 # This order and two-frame source-object scanout lag have separate exact
 # captured-pixel evidence in tools/verify_intro_actor_pixels.py.
 for slot in (1,2,0):
  actor=actors[frame-2,slot];r=emit(program,actor,0,actor['camera'])
  if r is None:continue
  group=r['attribute']&31
  for ty in range(r['height']):
   for tx in range(r['width']):
    code=(r['code']&~15)+((r['code']+tx)&15)+ty*16
    if code not in cache:cache[code]=decode_tile(gfx,code,16)
    for sy in range(16):
     for sx in range(16):
      pen=cache[code][sy][sx]
      if pen==15:continue
      x=tx*16+sx;y=ty*16+sy
      if r['attribute']&32:x=r['width']*16-1-x
      if r['attribute']&64:y=r['height']*16-1-y
      x+=r['x']-96;y+=r['y']-16
      if not(0<=x<320 and 0<=y<224) or (x,y) in plane:continue
      source=cps_rgb(word(ram,0x1c000+group*32+pen*2));old=min(oldplayer if group==0 else oldnpc,key=lambda c:color_error(source,c))
      colors=[source,old]
      for plan,lookup in zip(plans,lookups):
       m=lookup[code,group];colors.append(rgb(plan['cram'][m['page']][m['pens'][pen+1]]))
      plane[x,y]=(group,colors)
 def covered_by_text(x,y):
  col=(x+32)//8;row=(y+16)//8;a=0x10000+(row&31)*4+col*128+((row&32)<<8)
  code=word(ram,a);key=('text',code,col&1)
  if key not in cache:cache[key]=decode_tile(gfx,code,8,col&1)
  return cache[key][(y+16)%8][(x+32)%8]!=15
 sprite_images=[Image.new('RGB',(320,224),(32,32,32)) for _ in range(4)]
 context_images=[original.copy() for _ in range(4)];matched=0;masked=0;mismatch=[];group_errors={}
 for (x,y),(group,colors) in plane.items():
  for im,color in zip(sprite_images,colors):im.putpixel((x,y),color)
  if covered_by_text(x,y):masked+=1;continue
  if original.getpixel((x,y))!=colors[0]:mismatch.append((x,y,colors[0],original.getpixel((x,y))));continue
  matched+=1
  errors=group_errors.setdefault(group,{'pixels':0,'old_squared_error':0,'joint1_squared_error':0,'joint16_squared_error':0});errors['pixels']+=1
  for key,color in zip(('old_squared_error','joint1_squared_error','joint16_squared_error'),colors[1:]):errors[key]+=sum((a-b)**2 for a,b in zip(colors[0],color))
  for im,color in zip(context_images[1:],colors[1:]):im.putpixel((x,y),color)
 assert not mismatch,(frame,len(mismatch),mismatch[:5])
 # Four columns retain one source pixel per display pixel in this artifact.
 sheet=Image.new('RGB',(1280,500),(20,20,20));d=ImageDraw.Draw(sheet)
 titles=['Existing arcade capture / CPS sprite plane','Old dedicated NPC + font/player fit']+['Explicit remap: '+name for name in args.candidates]
 for col,title in enumerate(titles):
  d.text((col*320+3,3),title,fill='white');sheet.paste(sprite_images[col],(col*320,24));sheet.paste(context_images[col],(col*320,276))
 d.text((3,252),f'Frame {frame}: context below retains arcade backgrounds/text; only source-verified visible sprite colors change. Not a native frame.',fill='white')
 sheet.save(OUT/f'{args.prefix}-comparison-{frame}.png')
 roi=(88,112,192,208);zoom=Image.new('RGB',(416*4,412),(20,20,20));d=ImageDraw.Draw(zoom)
 for col,title in enumerate(titles):
  d.text((col*416+3,3),title,fill='white');zoom.paste(context_images[col].crop(roi).resize((416,384),Image.Resampling.NEAREST),(col*416,28))
 zoom.save(OUT/f'{args.prefix}-detail-{frame}.png')
 evidence.append({'frame':frame,'source_capture':str(capture_path),'source_capture_sha256':hashlib.sha256(capture_path.read_bytes()).hexdigest(),'crop':[32,0,352,224],'sprite_plane_opaque_pixels':len(plane),'scroll1_covered_pixels':masked,'visible_source_pixels_verified_exactly':matched,'mismatches':len(mismatch),'per_group_errors':group_errors,'error_field_candidates':dict(zip(('joint1_squared_error','joint16_squared_error'),args.candidates)),'geometry':'Original emitted actor records at frame-2; exact existing capture match, no spatial resampling except labeled 4x nearest detail','outputs':[f'{args.prefix}-comparison-{frame}.png',f'{args.prefix}-detail-{frame}.png']})
(OUT/f'{args.prefix}-comparison.json').write_text(json.dumps({'frames':evidence,'candidates':args.candidates,'scope':'Unchanged reference column; old/new sprite-color substitutions only in derived context columns. Source sprite geometry and visible colors match captured arcade PNGs exactly. Background/native CRAM coexistence and actual cartridge output are not asserted.'},indent=2)+'\n');print(json.dumps(evidence,indent=2))
