#!/usr/bin/env python3
"""Diagnostic source-faithful Scroll1 rectangle/page packing, immutable CRAM.

Geometry stays original8x8. One shared page per up-to4x4-cell Genesis sprite;
all source pens are re-evaluated from full CPS RGB. No background/object culling.
"""
from pathlib import Path
from collections import Counter
import argparse,ctypes as c,json,math,struct,zipfile
from PIL import Image,ImageDraw
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb
from native_palette_fit import color_error
from native_scroll1_palette_plan import source_cell
from audit_native_object_palette_fit import cram_rgb
args=argparse.ArgumentParser();args.add_argument('--hole-free',action='store_true');args.add_argument('--purple-font',action='store_true');args=args.parse_args()
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit';DEST=OUT/('scroll1-rectangle-'+('holefree' if args.hole_free else 'candidate')+('-purple' if args.purple_font else ''));DEST.mkdir(exist_ok=True)
data=json.loads((OUT/'scroll1_reuse_p3_w16.json').read_text());pages=[[cram_rgb(w) for w in page] for page in data['cram']]
if args.purple_font:
 # Candidate only: replace a duplicate white, retaining both immutable player
 # pages. Quantize the original font pen2 with its strict B>R>G source hue.
 from itertools import product
 source=cps_rgb(data['source_palette_words']['0'][2])
 anchor=min((p for p in product(range(0,239,34),repeat=3) if p[2]>p[0]>p[1]),key=lambda p:(color_error(source,p),p))
 pages[3][14]=anchor
with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
sourcepal={int(g):[cps_rgb(w) for w in words] for g,words in data['source_palette_words'].items()};tiles={};remaps={};errors={};sqerrors={};source_pixels={};opacity={}
for group,colors in sourcepal.items():
 for page in range(4):remaps[group,page]=[min(range(1,16),key=lambda p:(color_error(color,pages[page][p]),p)) for color in colors[:15]]+[0]
for entry in data['entries']:
 key=entry['code'],entry['source_group'],entry['layout'];pens=decode_tile(gfx,key[0],8,key[2]);source_pixels[key]=pens;hist=Counter(p for row in pens for p in row if p!=15);opacity[key]=bool(hist)
 errors[key]=[sum(n*color_error(sourcepal[key[1]][pen],pages[page][remaps[key[1],page][pen]]) for pen,n in hist.items()) for page in range(4)]
 sqerrors[key]=[sum(n*sum((a-b)**2 for a,b in zip(sourcepal[key[1]][pen],pages[page][remaps[key[1],page][pen]])) for pen,n in hist.items()) for page in range(4)]
 for flip in range(4):
  for page in range(4):
   oriented=[[remaps[key[1],page][pens[7-y if flip&2 else y][7-x if flip&1 else x]] for x in range(8)] for y in range(8)]
   tiles[key,flip,page]=genesis_tiles(oriented)
def cells(raw,base):
 result={}
 for y in range(28):
  for x in range(40):
   code,attr=source_cell(raw,x+4,y+2,base);key=code,attr&31,(x+4)&1
   if key in opacity and opacity[key]:result[x,y]=(key,(attr>>5)&3)
 return result
def rectangles(cellmap,blockw=4,blockh=4):
 out=[];bounds=[]
 if args.hole_free:
  remaining=set(cellmap)
  while remaining:
   bx,by=min(remaining,key=lambda p:(p[1],p[0]));choices=[]
   for h in range(1,min(4,28-by)+1):
    for w in range(1,min(4,40-bx)+1):
     points={(x,y) for y in range(by,by+h) for x in range(bx,bx+w)}
     if points<=remaining:choices.append((w*h,w,h,points))
   area,w,h,points=max(choices,key=lambda q:(q[0],q[1],q[2]));remaining-=points;bounds.append((bx,by,w,h))
 else:bounds=[(bx,by,blockw,blockh) for by in range(0,28,blockh) for bx in range(0,40,blockw)]
 for bx,by,blockw,blockh in bounds:
   points=[(x,y) for y in range(by,min(28,by+blockh)) for x in range(bx,min(40,bx+blockw)) if (x,y) in cellmap]
   if not points:continue
   x0=min(x for x,y in points);x1=max(x for x,y in points)+1;y0=min(y for x,y in points);y1=max(y for x,y in points)+1
   e=[sum(errors[cellmap[pos][0]][page] for pos in points) for page in range(4)]
   # Black panel pixels must share a consistent nearest black target. PAL0
   # lacks an opaque black pen; do not produce gray/black patches from ties.
   has_panel_black=any(any(p!=15 and sourcepal[cellmap[pos][0][1]][p] in ((0,0,0),(17,17,17)) for row in source_pixels[cellmap[pos][0]] for p in row) for pos in points)
   allowed=[page for page in range(4) if not args.purple_font or not has_panel_black or (0,0,0) in pages[page][1:]]
   page=min(allowed,key=lambda page:(e[page],page));packed=[]
   for x in range(x0,x1):
    for y in range(y0,y1):
     packed.append(tiles[*cellmap[x,y],page] if (x,y) in cellmap else bytes(32))
   out.append({'x':x0*8,'y':y0*8,'width':x1-x0,'height':y1-y0,'page':page,'packed':b''.join(packed),'error':e[page],'rgb_squared_error':sum(sqerrors[cellmap[pos][0]][page] for pos in points),'cells':len(points)})
 return out
class Budget(c.Structure):_fields_=[('count',c.c_uint16),('sprites',c.c_uint8*224),('pixels',c.c_uint16*224)]
class OP(c.Structure):_fields_=[('x',c.c_int16),('y',c.c_int16),('code',c.c_uint16),('sourceAttribute',c.c_uint16),('attribute',c.c_uint16)]
class Result(c.Structure):_fields_=[('status',c.c_uint8),('pieces',c.c_uint16),('records',c.c_uint16),('record',c.c_uint16),('column',c.c_uint16),('row',c.c_uint16)]
Asset=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_int16,c.c_int16,c.POINTER(c.c_uint16));objcache={}
@Asset
def object_asset(_,code,attr,x,y,out):
 if code not in objcache:objcache[code]=any(p!=15 for row in decode_tile(gfx,code,16) for p in row)
 out[0]=0;return 1 if objcache[code] else 2
lib=c.CDLL(str(OUT/'scroll1-test.dylib'));lib.willowScroll1TestObjects.argtypes=[c.POINTER(c.c_uint16),c.POINTER(Budget),c.POINTER(OP),Asset,c.c_void_p,c.POINTER(Result)]
def budget_for(rects):
 b=Budget();b.count=len(rects)
 for r in rects:
  for y in range(r['y'],r['y']+r['height']*8):b.sprites[y]+=1;b.pixels[y]+=r['width']*8
 status=3 if b.count>80 else 4 if max(b.sprites)>20 else 5 if max(b.pixels)>320 else 0
 return b,status
cases=[];combined=[];allpatterns=set();pixelchecks=0;summaries={};allmaps={};allrects={};occlusion=[]
for capture in data['captures']:
 raw=Path(capture['path']).read_bytes();cellmap=cells(raw,capture['base']);rects=rectangles(cellmap);b,status=budget_for(rects);allmaps[capture['path']]=cellmap;allrects[capture['path']]=rects
 patterns=sum(len(r['packed'])//32 for r in rects)
 # Verify packed VDP column-major sprite pixels against original CPS geometry.
 for r in rects:
  for yy in range(r['height']*8):
   for xx in range(r['width']*8):
    x=r['x']+xx;y=r['y']+yy;cell=cellmap.get((x//8,y//8));at=((xx//8)*r['height']+yy//8)*32+(yy%8)*4+(xx%8)//2;pen=(r['packed'][at]>>(0 if xx&1 else 4))&15
    source=15 if cell is None else source_pixels[cell[0]][7-y%8 if cell[1]&2 else y%8][7-x%8 if cell[1]&1 else x%8]
    assert pen==(0 if cell is None else remaps[cell[0][1],r['page']][source]);assert (pen==0)==(source==15);pixelchecks+=1
  allpatterns.update(r['packed'][i:i+32] for i in range(0,len(r['packed']),32))
 case={'frame':capture['frame'],'kind':capture['kind'],'status':status,'text_entries':b.count,'staging_tiles':patterns,'max_line_sprites':max(b.sprites),'max_line_pixels':max(b.pixels),'source_pixels':sum(sum(p!=15 for row in source_pixels[k] for p in row) for k,f in cellmap.values()),'rgb_squared_error':sum(r['rgb_squared_error'] for r in rects),'weighted_error':sum(r['error'] for r in rects),'individual_cell_weighted_error':sum(min(errors[k]) for k,f in cellmap.values())};case['rgb_rmse']=math.sqrt(case['rgb_squared_error']/max(1,3*case['source_pixels']));cases.append(case)
 if capture['kind']=='intro_vram':
  for bank in (0,0x8000):
   b2=Budget.from_buffer_copy(bytes(b));result=Result();pieces=(OP*80)();words=(c.c_uint16*1024)(*struct.unpack('>1024H',raw[bank:bank+0x800]))
   if not status:lib.willowScroll1TestObjects(words,c.byref(b2),pieces,object_asset,None,c.byref(result))
   combined.append({'frame':capture['frame'],'bank':bank,'status':status or result.status,'text_entries':b.count,'objects_prepared':result.pieces,'total_entries':b2.count,'max_line_sprites':max(b2.sprites),'max_line_pixels':max(b2.pixels)})
   if capture['frame'] in (1020,1140,1200,1440):
    proof=json.loads((OUT/f'source-composition-{capture["frame"]}.json').read_text());assert not proof['mismatches']
    # Actual captured object bank must equal the bank used in the exact
    # source composition proof before accepting its layer occlusion order.
    if list(struct.unpack('>12H',raw[bank:bank+24]))!=[v for r in proof['object_records'] for v in r]:continue
    source_mask=set()
    for (cx,cy),(key,flip) in cellmap.items():
     for y in range(8):
      for x in range(8):
       if source_pixels[key][7-y if flip&2 else y][7-x if flip&1 else x]!=15:source_mask.add((cx*8+x,cy*8+y))
    ob=Budget();pieces=(OP*80)();res=Result();lib.willowScroll1TestObjects(words,c.byref(ob),pieces,object_asset,None,c.byref(res));assert not res.status
    plans=[];hidden=0;split=0;quarter_hidden=0
    for piece in pieces[:res.pieces]:
     original=decode_tile(gfx,piece.code,16);opaque=set()
     for y in range(16):
      for x in range(16):
       px=piece.x+x;py=piece.y+y
       if 0<=px<320 and 0<=py<224 and original[15-y if piece.attribute&0x1000 else y][15-x if piece.attribute&0x800 else x]!=15:opaque.add((px,py))
     if not opaque-source_mask:hidden+=1;continue
     if not opaque&source_mask:plans.append((piece.x,piece.y,16,16));continue
     split+=1
     for y in (0,8):
      for x in (0,8):
       points={p for p in opaque if piece.x+x<=p[0]<piece.x+x+8 and piece.y+y<=p[1]<piece.y+y+8}
       if not points-source_mask:quarter_hidden+=1;continue
       plans.append((piece.x+x,piece.y+y,8,8))
    ob=Budget.from_buffer_copy(bytes(b));ob.count+=len(plans)
    for x,y,w,h in plans:
     for line in range(max(0,y),min(224,y+h)):ob.sprites[line]+=1;ob.pixels[line]+=w
    ostatus=3 if ob.count>80 else 4 if max(ob.sprites)>20 else 5 if max(ob.pixels)>320 else 0
    occlusion.append({'frame':capture['frame'],'bank':bank,'source_priority_pixel_proof':str(OUT/f'source-composition-{capture["frame"]}.json'),'original_object_entries':res.pieces,'fully_hidden16x16':hidden,'split16x16':split,'hidden_or_empty8x8_quarters':quarter_hidden,'object_entries':len(plans),'text_entries':b.count,'total_entries':ob.count,'max_line_sprites':max(ob.sprites),'max_line_pixels':max(ob.pixels),'status':ostatus})
# Source image/candidate replacements preserve all non-Scroll1 reference pixels.
frames=(1020,1200,1440,1590);sheet=Image.new('RGB',(320*3,248*len(frames)),(32,32,32));draw=ImageDraw.Draw(sheet);comparisons=[]
for n,frame in enumerate(frames):
 capture=next(c for c in data['captures'] if c['kind']=='intro_vram' and c['frame']==frame);path=capture['path'];cellmap=allmaps[path];rects=allrects[path];source=Image.open(ROOT/f'build/arcade-trace/natural-intro-{frame}.png').convert('RGB').crop((32,0,352,224));old=source.copy();new=source.copy();mismatches=0;visible=0
 for (cx,cy),(key,flip) in cellmap.items():
  page=min(range(4),key=lambda page:(errors[key][page],page))
  for y in range(8):
   for x in range(8):
    pen=source_pixels[key][7-y if flip&2 else y][7-x if flip&1 else x]
    if pen==15:continue
    pos=cx*8+x,cy*8+y;original=sourcepal[key[1]][pen];mismatches+=source.getpixel(pos)!=original;visible+=1
    old.putpixel(pos,pages[page][remaps[key[1],page][pen]])
 for r in rects:
  for y in range(r['height']*8):
   for x in range(r['width']*8):
    at=((x//8)*r['height']+y//8)*32+(y%8)*4+(x%8)//2;pen=(r['packed'][at]>>(0 if x&1 else 4))&15
    if pen:new.putpixel((r['x']+x,r['y']+y),pages[r['page']][pen])
 comparisons.append({'frame':frame,'source_scroll1_pixels':visible,'source_capture_mismatches':mismatches});assert not mismatches
 for col,(label,img) in enumerate((('CPS source',source),('Per-glyph best page',old),('Shared4x4 block page',new))):
  draw.text((col*320+4,n*248+3),f'{frame}: {label}',fill='white');sheet.paste(img,(col*320,n*248+24))
 new.save(DEST/f'context-{frame}.png')
sheet.resize((1920,len(frames)*496),Image.Resampling.NEAREST).save(DEST/'comparison.png')
report={'scope':'Diagnostic source-original320px crop. Primary combined gate includes ALL original object pieces. Separate occlusion experiment requires exact source composition proof and removes only provably hidden original object pixels. Not admitted runtime code or full-game coverage.','hole_free':args.hole_free,'purple_font_candidate':args.purple_font,'cram_rgb':pages,'text_cases':len(cases),'text_failures':[r for r in cases if r['status']],'text_peak_entries':max(r['text_entries'] for r in cases),'text_peak_staging_tiles':max(r['staging_tiles'] for r in cases),'text_peak_line_sprites':max(r['max_line_sprites'] for r in cases),'text_peak_line_pixels':max(r['max_line_pixels'] for r in cases),'combined_cases':len(combined),'combined_failures':[r for r in combined if r['status']],'combined_peak_entries':max(r['total_entries'] for r in combined),'combined_peak_line_sprites':max(r['max_line_sprites'] for r in combined),'combined_peak_line_pixels':max(r['max_line_pixels'] for r in combined),'source_proven_occlusion_cases':occlusion,'pixel_checks':pixelchecks,'unique_patterns_used':len(allpatterns),'comparisons':comparisons,'cases':cases,'combined':combined}
(DEST/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k not in ('cases','combined','text_failures','combined_failures')},indent=2));print('failures',len(report['text_failures']),len(report['combined_failures']))
