#!/usr/bin/env python3
"""Source-proven frame1200 Window/PlaneB allocation, static fixture only."""
from pathlib import Path
from collections import Counter
from itertools import product
import ctypes as c,hashlib,json,math,runpy,struct,sys
from PIL import Image,ImageDraw
from cps1_gfx import decode_tile,genesis_tiles
from cps1_palette import cps_rgb
from native_scroll1_palette_plan import source_cell
from native_palette_fit import color_error
from audit_native_object_palette_fit import cram_rgb,cram_word
from fit_native_object_palette_anchors import is_neutral_stone,preserves_neutral
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit';DEST=OUT/'frame1200_window_v1';DEST.mkdir(exist_ok=True)
proof=runpy.run_path(str(ROOT/'tools/prove_native_opening_frame.py'),init_globals={'FRAME':1200,'SOURCE_BANK':0})
assert json.loads((OUT/'source-composition-1200.json').read_text())['mismatches']==0
obj=json.loads((OUT/'object_palette_reuse_p3_w16.json').read_text());ui=json.loads((OUT/'scroll1_reuse_p3_w16.json').read_text());oldpages=[[cram_rgb(w) for w in p] for p in obj['cram']]
ram=proof['ram'];gfx=proof['g'];layers=proof['layers'];object_palette={int(g):[cps_rgb(w) for w in words] for g,words in obj['source_palettes'].items()};ui_palette={int(g):[cps_rgb(w) for w in words] for g,words in ui['source_palette_words'].items()}
anchor_source=ui_palette[0][2];font_anchor=min((p for p in product(range(0,239,34),repeat=3) if p[2]>p[0]>p[1]),key=lambda p:(color_error(anchor_source,p),p));font_dark_source=ui_palette[0][7];font_dark=min((p for p in product(range(0,239,34),repeat=3) if p[2]>p[0]>p[1]),key=lambda p:(color_error(font_dark_source,p),p));anchors={}
for a in obj['anchors']:anchors[a['group'],tuple(a['source_rgb'])]=tuple(a['hardware_rgb'])
def source_bg(layer,x,y):
 pen,group=layers.pen(layer,x+32,y-240)
 return None if pen==15 else proof['pal'][layer][group][pen]
ui_cells={};ui_last_y=0
for ty in range(28):
 for tx in range(40):
  code,attr=source_cell(ram,tx+4,ty+2,0x10000)
  if not 0x7000<=code<=0x7fff:continue
  source=decode_tile(gfx,code,8,(tx+4)&1);pixels=[[None if (pen:=source[7-y if attr&64 else y][7-x if attr&32 else x])==15 else ui_palette[attr&31][pen] for x in range(8)] for y in range(8)]
  if any(p is not None for row in pixels for p in row):ui_cells[tx,ty]={'pixels':pixels,'group':attr&31,'code':code,'attribute':attr};ui_last_y=max(ui_last_y,(ty+1)*8)
assert ui_last_y==80
bg_cells=[]
for ty in range(28):
 for tx in range(40):
  near=[[source_bg(2,tx*8+x,ty*8+y) for x in range(8)] for y in range(8)];far=[[source_bg(3,tx*8+x,ty*8+y) for x in range(8)] for y in range(8)]
  merged=[[near[y][x] if near[y][x] is not None else far[y][x] for x in range(8)] for y in range(8)]
  if ty*8<ui_last_y:bg_cells.append({'kind':'composed','table':'b','x':tx,'y':ty,'pixels':merged})
  else:
   bg_cells.append({'kind':'near','table':'a','x':tx,'y':ty,'pixels':near});bg_cells.append({'kind':'far','table':'b','x':tx,'y':ty,'pixels':far})
# C bridge supplies original16x16 pieces/priority/order; only allocation changes.
class Budget(c.Structure):_fields_=[('count',c.c_uint16),('sprites',c.c_uint8*224),('pixels',c.c_uint16*224)]
class Piece(c.Structure):_fields_=[('x',c.c_int16),('y',c.c_int16),('code',c.c_uint16),('sourceAttribute',c.c_uint16),('attribute',c.c_uint16)]
class Result(c.Structure):_fields_=[('status',c.c_uint8),('pieces',c.c_uint16),('records',c.c_uint16),('record',c.c_uint16),('column',c.c_uint16),('row',c.c_uint16)]
Asset=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_int16,c.c_int16,c.POINTER(c.c_uint16));objects={}
@Asset
def collect(_,code,attr,x,y,out):
 if (code,attr&31) not in objects:
  pixels=decode_tile(gfx,code,16);objects[code,attr&31]={'code':code,'group':attr&31,'pens':pixels,'pixels':[[None if pen==15 else object_palette[attr&31][pen] for pen in row] for row in pixels]}
 if not any(v is not None for row in objects[code,attr&31]['pixels'] for v in row):return 2
 out[0]=0;return 1
lib=c.CDLL(str(OUT/'scroll1-test.dylib'));lib.willowScroll1TestObjects.argtypes=[c.POINTER(c.c_uint16),c.POINTER(Budget),c.POINTER(Piece),Asset,c.c_void_p,c.POINTER(Result)]
words=(c.c_uint16*1024)(*struct.unpack('>1024H',ram[:0x800]));budget=Budget();pieces=(Piece*80)();result=Result();lib.willowScroll1TestObjects(words,c.byref(budget),pieces,collect,None,c.byref(result));assert not result.status

def histogram(pixels):return Counter(p for row in pixels for p in row if p is not None)
for cell in bg_cells:cell['hist']=histogram(cell['pixels'])
for cell in list(ui_cells.values())+list(objects.values()):cell['hist']=histogram(cell['pixels'])
def fit_cell(cell,pages,kind):
 hist=cell['hist'];group=cell.get('group',-1);forced={color:anchors[group,color] for color in hist if (group,color) in anchors} if kind=='object' else {}
 allowed=[group] if kind=='object' and group in (0,1) else [3] if forced else range(4)
 lookup={};scores={}
 for page in allowed:
  mapped={};valid=True
  for color in hist:
   options=list(range(1,16))
   if kind=='background' and is_neutral_stone(color):options=[p for p in options if preserves_neutral(color,pages[page][p])]
   if kind=='object' and color in forced:options=[p for p in options if pages[page][p]==forced[color]]
   if kind=='text' and group==0 and color[2]>color[0]>color[1]:options=[p for p in options if pages[page][p][2]>pages[page][p][0]>pages[page][p][1]]
   if kind=='text' and color in ((0,0,0),(17,17,17)) and (group==0 or all(c in ((0,0,0),(17,17,17)) for c in hist)):options=[p for p in options if pages[page][p]==(0,0,0)]
   if not options:valid=False;break
   mapped[color]=min(options,key=lambda p:(color_error(color,pages[page][p]),p))
  if valid:lookup[page]=mapped;scores[page]=sum(n*color_error(color,pages[page][mapped[color]]) for color,n in hist.items())
 if not scores:return None
 page=min(scores,key=lambda p:(scores[p],p));mapping=lookup[page]
 return {'page':page,'pens':mapping,'weighted_error':scores[page],'pixels':sum(hist.values()),'rgb_squared_error':sum(n*sum((a-b)**2 for a,b in zip(color,pages[page][mapping[color]])) for color,n in hist.items())}
def metric(items,fits):
 pixels=sum(f['pixels'] for f in fits);rgb=sum(f['rgb_squared_error'] for f in fits);weighted=sum(f['weighted_error'] for f in fits)
 return {'pixels':pixels,'rgb_rmse':math.sqrt(rgb/max(1,3*pixels)),'weighted_error':weighted,'weighted_error_mean':weighted/max(1,pixels)}
variants=[]
for slot,dark_slot in ((a,b) for a in range(8,16) for b in range(a+1,16)):
 pages=[list(p) for p in oldpages];pages[3][slot]=font_anchor;pages[3][dark_slot]=font_dark
 bgfit=[fit_cell(cell,pages,'background') for cell in bg_cells];objfit=[fit_cell(cell,pages,'object') for cell in objects.values()];textfit=[fit_cell(cell,pages,'text') for cell in ui_cells.values()]
 if any(f is None for f in bgfit+objfit+textfit):continue
 bgm=metric(bg_cells,bgfit);objm=metric(list(objects.values()),objfit);uim=metric(list(ui_cells.values()),textfit)
 variants.append({'slot':slot,'dark_slot':dark_slot,'pages':pages,'background':bgm,'objects':objm,'text':uim,'objective':bgm['weighted_error']+16*objm['weighted_error']+4*uim['weighted_error']})
chosen=min(variants,key=lambda v:v['objective']);pages=chosen['pages'];assert pages[:2]==oldpages[:2]
(DEST/'palette-variants.json').write_text(json.dumps({'font_anchor_source':anchor_source,'font_anchor_hardware':font_anchor,'replaced_pal3_slot':chosen['slot'],'font_dark_source':font_dark_source,'font_dark_hardware':font_dark,'dark_slot':chosen['dark_slot'],'candidates':variants,'scope':'Frame1200 candidate only; slot selection weighs background1, source NPC/object16, text4. Both player pages unchanged; all original robe/wood anchors preserved.'},indent=2)+'\n')
vram=bytearray(65536);next_tile=16;pattern_ids={bytes(32):0};block_ids={};ownership=[]
def tile(packed):
 global next_tile
 if packed not in pattern_ids:
  assert next_tile<1536;pattern_ids[packed]=next_tile;vram[next_tile*32:(next_tile+1)*32]=packed;next_tile+=1
 return pattern_ids[packed]
def encode(cell,fit):return genesis_tiles([[0 if color is None else fit['pens'][color] for color in row] for row in cell['pixels']])
background_fits=[]
for cell in bg_cells:
 fit=fit_cell(cell,pages,'background');background_fits.append(fit);physical=tile(encode(cell,fit));table=0xc000 if cell['table']=='a' else 0xe000;struct.pack_into('>H',vram,table+(cell['y']*64+cell['x'])*2,(fit['page']<<13)|physical)
ownership.append({'kind':'background','first_tile':16,'tiles':next_tile-16});start=next_tile
for (x,y),cell in ui_cells.items():
 fit=fit_cell(cell,pages,'text');physical=tile(encode(cell,fit));struct.pack_into('>H',vram,0xd000+(y*64+x)*2,0x8000|(fit['page']<<13)|physical)
ownership.append({'kind':'window_scroll1','first_tile':start,'tiles':next_tile-start});start=next_tile
@Asset
def acquire(_,code,attr,x,y,out):
 global next_tile
 cell=objects[code,attr&31]
 if not cell['hist']:return 2
 fit=fit_cell(cell,pages,'object');packed=encode(cell,fit)
 # genesis_tiles emits row-major quadrant order: convert16x16 to the VDP's
 # column-major sprite order TL,BL,TR,BR.8x8 cells remain unchanged.
 packed=packed[:32]+packed[64:96]+packed[32:64]+packed[96:128]
 key=code,attr&31
 if key not in block_ids:
  assert next_tile+4<=1536;block_ids[key]=next_tile;vram[next_tile*32:(next_tile+4)*32]=packed;next_tile+=4
 out[0]=(fit['page']<<13)|block_ids[key];return 1
budget=Budget();pieces=(Piece*80)();result=Result();lib.willowScroll1TestObjects(words,c.byref(budget),pieces,acquire,None,c.byref(result));assert not result.status
ownership.append({'kind':'objects','first_tile':start,'tiles':next_tile-start});sat=[]
# Adjacent source-keyed resident asset binding; no captured coordinates or
# expected SAT words are consulted by the consumer. Include inspected blanks.
source_capture=ROOT/'build/arcade-trace/intro-vram-1200.bin'
asset_entries=[]
for (code,group),cell in sorted(objects.items()):
 if not cell['hist']:asset_entries.append({'code':code,'source_group':group,'attribute':0,'status':2})
 else:
  fit=fit_cell(cell,pages,'object')
  asset_entries.append({'code':code,'source_group':group,'attribute':(fit['page']<<13)|block_ids[code,group],'status':1})
binding={'source_capture':str(source_capture.relative_to(ROOT)),'source_capture_sha256':hashlib.sha256(source_capture.read_bytes()).hexdigest(),'bank_offset':0,'bank_bytes':2048,'bank_sha256':hashlib.sha256(ram[:2048]).hexdigest(),'entries':asset_entries,'text_sat_entries':0,'scope':'All visible original code/group keys inspected by the native bridge for the complete captured bank; transparent entries are original all-pen15 tiles. Fixed resident VRAM attributes, no source flips/priority bits or coordinates.'}
(DEST/'asset-binding.json').write_text(json.dumps(binding,indent=2)+'\n')
for n,p in enumerate(pieces[:result.pieces]):
 row=[p.y+128,0x500|(n+1 if n+1<result.pieces else 0),p.attribute,p.x+128];sat.append(row);struct.pack_into('>4H',vram,0xf400+n*8,*row)
# Full pre-fit composition was exact above. Check every encoded pattern pixel
# against its explicitly selected source pen, and independently render final
# VDP buffers in actual Window-high > sprite-low > BG-low order.
cram=[cram_word(color) for page in pages for color in page];reg=bytearray(24)
for n,v in {0:4,1:0x54,2:0x30,3:0x34,4:7,5:0x7a,7:0,10:0xff,12:0x81,13:0x3c,15:2,16:1,17:0,18:ui_last_y//8}.items():reg[n]=v
def pixel(attr,x,y):
 if attr&0x800:x=7-x
 if attr&0x1000:y=7-y
 at=(attr&0x7ff)*32+y*4+x//2;return (vram[at]>>(0 if x&1 else 4))&15
def word(table,x,y):return int.from_bytes(vram[table+((y//8)*64+x//8)*2:table+((y//8)*64+x//8)*2+2],'big')
expected=Image.new('RGB',(320,224));source_background=Image.new('RGB',(320,224));fitted_background=Image.new('RGB',(320,224))
for y in range(224):
 for x in range(320):
  color=(0,0,0)
  for table in (0xe000,0xc000):
   if table==0xc000 and y<ui_last_y:continue
   attr=word(table,x,y);pen=pixel(attr,x&7,y&7)
   if pen:color=pages[(attr>>13)&3][pen]
  expected.putpixel((x,y),color);fitted_background.putpixel((x,y),color);source_background.putpixel((x,y),source_bg(2,x,y) or source_bg(3,x,y) or (0,0,0))
covered=set();object_pixel_checks=0
for p in pieces[:result.pieces]:
 cell=objects[p.code,p.sourceAttribute&31];fit=fit_cell(cell,pages,'object')
 for y in range(16):
  for x in range(16):
   sx=15-x if p.attribute&0x800 else x;sy=15-y if p.attribute&0x1000 else y;attr=(p.attribute&~0x1fff)|((p.attribute&0x7ff)+(sx//8)*2+sy//8);pen=pixel(attr,sx&7,sy&7);source=cell['pixels'][sy][sx];assert pen==(0 if source is None else fit['pens'][source]);object_pixel_checks+=1
   px=p.x+x;py=p.y+y
   if pen and 0<=px<320 and 0<=py<224 and (px,py) not in covered:expected.putpixel((px,py),pages[(attr>>13)&3][pen]);covered.add((px,py))
text_pixel_checks=0
for y in range(ui_last_y):
 for x in range(320):
  attr=word(0xd000,x,y);pen=pixel(attr,x&7,y&7);cell=ui_cells.get((x//8,y//8));source=None if cell is None else cell['pixels'][y&7][x&7];fit=None if cell is None else fit_cell(cell,pages,'text');assert pen==(0 if source is None else fit['pens'][source]);text_pixel_checks+=1
  if pen:expected.putpixel((x,y),pages[(attr>>13)&3][pen])
source_background.save(DEST/'source-background.png');fitted_background.save(DEST/'fitted-background.png');expected.save(DEST/'expected-rgb888.png')
reference=Image.open(OUT/'source-composition-1200.png');sheet=Image.new('RGB',(960,248),(24,24,24));draw=ImageDraw.Draw(sheet)
for n,(title,img) in enumerate((('CPS source1200',reference),('Window candidate / hardwareRGB',expected),('Background after composition',fitted_background))):draw.text((n*320+4,4),title,fill='white');sheet.paste(img,(n*320,24))
sheet.resize((1920,496),Image.Resampling.NEAREST).save(DEST/'comparison.png')
def crop_metric(a,b,y0,y1):
 source=[a.getpixel((x,y)) for y in range(y0,y1) for x in range(320)];target=[b.getpixel((x,y)) for y in range(y0,y1) for x in range(320)];n=len(source)
 return {'pixels':n,'source_colors':len(set(source)),'output_colors':len(set(target)),'rgb_rmse':math.sqrt(sum(sum((s-t)**2 for s,t in zip(a,b)) for a,b in zip(source,target))/(3*n))}
# Composition cost isolated against independent per-layer per8x8 fitting with
# the SAME chosen palette. This excludes the font-anchor palette change.
independent=Image.new('RGB',(320,224))
for ty in range(28):
 for tx in range(40):
  cells=[{'pixels':[[source_bg(layer,tx*8+x,ty*8+y) for x in range(8)] for y in range(8)]} for layer in (3,2)]
  for cell in cells:
   cell['hist']=histogram(cell['pixels']);fit=fit_cell(cell,pages,'background')
   for y in range(8):
    for x in range(8):
     color=cell['pixels'][y][x]
     if color is not None:independent.putpixel((tx*8+x,ty*8+y),pages[fit['page']][fit['pens'][color]])
buffers={'vram.bin':bytes(vram),'cram.bin':struct.pack('>64H',*cram),'vsram.bin':bytes(80),'registers.bin':bytes(reg)}
for name,content in buffers.items():
 if (DEST/name).exists():assert (DEST/name).read_bytes()==content,'Frozen1200 VDP buffers changed'
 else:(DEST/name).write_bytes(content)
manifest={'expected_image':'expected-rgb888.png','frame':1200,'source_object_bank':0,'source_composition_pixels_verified':71680,'source_composition_mismatches':0,'priority':'Source-proven Scroll1 > objects > Scroll2 > Scroll3. Window tiles HIGH, sprites LOW, backgrounds LOW; Window transparent pixels reveal sprites then composed PlaneB, never a fake opaque background in front of sprites.','window_region':[0,0,320,ui_last_y],'source_crop':[32,0,352,224],'source_geometry':'Original8x8 Scroll1 and16x16 object pieces. No resampling or object culling. Only two low-priority backgrounds precomposed under the Window.','font_anchor':{'source_rgb':anchor_source,'hardware_rgb':font_anchor,'replaced_pal3_slot':chosen['slot'],'dark_source_rgb':font_dark_source,'dark_hardware_rgb':font_dark,'replaced_pal3_dark_slot':chosen['dark_slot']},'chosen_palette_metrics':chosen,'background_composition_cost':{'composed_window_region':crop_metric(source_background,fitted_background,0,ui_last_y),'independent_same_palette_window_region':crop_metric(source_background,independent,0,ui_last_y),'full_background':crop_metric(source_background,fitted_background,0,224)},'object_pixel_checks':object_pixel_checks,'text_pixel_checks':text_pixel_checks,'ownership':ownership,'highest_pattern_tile':next_tile-1,'pattern_ceiling_tile':1535,'plane_a':[0xc000,0xcfff],'window':[0xd000,0xdfff],'plane_b':[0xe000,0xefff],'hscroll':[0xf000,0xf003],'sat':[0xf400,0xf67f],'sat_entries':result.pieces,'max_line_sprites':max(budget.sprites),'max_line_pixels':max(budget.pixels),'both_player_pages_unchanged':True,'buffers':{name:{'bytes':len(content),'sha256':hashlib.sha256(content).hexdigest()} for name,content in buffers.items()},'scope':'Static frame1200 fixture, not live gameplay, source timing, whole-game priority or performance admission. Independent actual VDP capture still required.'}
if not (DEST/'manifest.json').exists():(DEST/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({k:v for k,v in manifest.items() if k!='chosen_palette_metrics'},indent=2))
