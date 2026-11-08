#!/usr/bin/env python3
"""Exercise immutable Scroll1 remaps, shared epochs and combined SAT budgets.

Also materialize the pixel-proven source frame1020 as complete VDP buffers for
an independent native68000 hardware fixture. This is not production gameplay.
"""
from pathlib import Path
import ctypes as c
import hashlib,json,runpy,struct,subprocess,zipfile
from PIL import Image
from cps1_gfx import assemble_region,decode_tile
from native_scroll1_palette_plan import source_cell
from cps1_palette import cps_rgb

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit'
class Source(c.Structure):_fields_=[('base',c.c_uint16),('words',c.c_uint16*16)]
class Owner(c.Structure):_fields_=[('cram',c.POINTER(c.c_uint16)),('sources',c.POINTER(Source)),('count',c.c_uint16),('required',c.c_uint16)]
class Lease(c.Structure):_fields_=[('plan',c.POINTER(Owner)),('epoch',c.POINTER(c.c_uint32)),('generation',c.c_uint32)]
class Frame(c.Structure):_fields_=[('lease',c.POINTER(Lease)),('generation',c.c_uint32),('ready',c.c_uint16)]
class Entry(c.Structure):_fields_=[('code',c.c_uint16),('pattern',c.c_uint16*4),('group',c.c_uint8),('layout',c.c_uint8),('map',c.c_uint8)]
class Map(c.Structure):_fields_=[('words',c.c_uint16*16),('group',c.c_uint8),('page',c.c_uint8),('pens',c.c_uint8*16)]
class Plan(c.Structure):_fields_=[('entries',c.POINTER(Entry)),('maps',c.POINTER(Map)),('patterns',c.POINTER(c.c_uint8)),('cram',c.POINTER(c.c_uint16)),('count',c.c_uint16),('patternCount',c.c_uint16),('mapCount',c.c_uint16)]
Read=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.POINTER(c.c_uint16))
Acquire=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.POINTER(Plan),c.c_uint16,c.POINTER(c.c_uint8),c.POINTER(c.c_uint16))
class Binding(c.Structure):_fields_=[('plan',c.POINTER(Plan)),('lease',c.POINTER(Lease)),('acquire',Acquire),('context',c.c_void_p),('generation',c.c_uint32),('groups',c.c_uint32)]
class Budget(c.Structure):_fields_=[('count',c.c_uint16),('sprites',c.c_uint8*224),('pixels',c.c_uint16*224)]
class Piece(c.Structure):_fields_=[('x',c.c_int16),('y',c.c_int16),('attribute',c.c_uint16),('width',c.c_uint8)]
class ObjectPiece(c.Structure):_fields_=[('x',c.c_int16),('y',c.c_int16),('code',c.c_uint16),('sourceAttribute',c.c_uint16),('attribute',c.c_uint16)]
class Result(c.Structure):_fields_=[('status',c.c_uint8),('pieces',c.c_uint16),('records',c.c_uint16),('record',c.c_uint16),('column',c.c_uint16),('row',c.c_uint16)]
Asset=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_int16,c.c_int16,c.POINTER(c.c_uint16))
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',str(ROOT/'tools/test_native_scroll1_tiles.c'),'-o',str(OUT/'scroll1-test.dylib')],check=True)
lib=c.CDLL(str(OUT/'scroll1-test.dylib'))
def signature(name,args):
 f=getattr(lib,name);f.argtypes=args;f.restype=c.c_uint;return f
valid=signature('willowScroll1TestValid',[c.POINTER(Plan)])
activate=signature('willowPresentationTestActivate',[c.POINTER(Lease),c.POINTER(Owner),c.POINTER(c.c_uint32),Read,c.c_void_p,c.POINTER(c.c_uint16)])
current=signature('willowPresentationTestCurrent',[c.POINTER(Lease)])
begin=signature('willowPresentationTestBegin',[c.POINTER(Frame),c.POINTER(Lease)])
prepared=signature('willowPresentationTestPrepared',[c.POINTER(Frame),c.c_uint16])
publish=signature('willowPresentationTestCanPublish',[c.POINTER(Frame)])
bind=signature('willowScroll1TestBind',[c.POINTER(Binding),c.POINTER(Plan),c.POINTER(Lease),Acquire,c.c_void_p])
resolve=signature('willowScroll1TestResolve',[c.POINTER(Binding),c.c_uint16,c.c_uint16,c.c_uint8,c.c_uint16,c.POINTER(c.c_uint16)])
row_plan=signature('willowScroll1TestRow',[c.POINTER(c.c_uint16),c.POINTER(c.c_uint8),c.c_int16,c.POINTER(Budget),c.POINTER(Piece),c.c_uint16,c.POINTER(c.c_uint16)])
sat_words=signature('willowScroll1TestWords',[c.POINTER(Piece),c.c_uint16,c.POINTER(c.c_uint16)])
lib.willowScroll1TestObjects.argtypes=[c.POINTER(c.c_uint16),c.POINTER(Budget),c.POINTER(ObjectPiece),Asset,c.c_void_p,c.POINTER(Result)]

data=json.loads((OUT/'scroll1_reuse_p3_w16.json').read_text());blob=(OUT/'scroll1_reuse_p3_w16.bin').read_bytes()
assert hashlib.sha256(blob).hexdigest()==data['binary_sha256']
entries=(Entry*len(data['entries']))(*(Entry(e['code'],(c.c_uint16*4)(*e['patterns_by_source_flip']),e['source_group'],e['layout'],e['map']) for e in data['entries']))
maps=(Map*len(data['maps']))(*(Map((c.c_uint16*16)(*m['source_words']),m['source_group'],m['page'],(c.c_uint8*16)(*m['pens'])) for m in data['maps']))
patterns=(c.c_uint8*len(blob)).from_buffer_copy(blob);cram=(c.c_uint16*64)(*(w for page in data['cram'] for w in page))
plan=Plan(entries,maps,patterns,cram,len(entries),len(blob)//32,len(maps));assert valid(c.byref(plan))==1
source_values={0x200+int(group)*16+pen:word for group,words in data['source_palette_words'].items() for pen,word in enumerate(words)}
source_groups=sorted((0x200+int(group)*16,words) for group,words in data['source_palette_words'].items())
sources=(Source*len(source_groups))(*(Source(base,(c.c_uint16*16)(*words)) for base,words in source_groups))
owner=Owner(cram,sources,len(sources),7);lease=Lease();epoch=c.c_uint32(1);read_mode=[0];cache_mode=[0];uploaded={};slot=[None];cache_calls=[0]
@Read
def read(_,index,out):
 if read_mode[0]==1:return 0
 out[0]=source_values[index]^(1 if read_mode[0]==2 else 0)
 if read_mode[0]==3:epoch.value+=1
 return 1
@Acquire
def acquire(_,descriptor,pattern,source,out):
 cache_calls[0]+=1
 assert c.addressof(descriptor.contents)==c.addressof(plan)
 assert c.string_at(source,32)==blob[pattern*32:(pattern+1)*32]
 if cache_mode[0]==1:return 0
 out[0]=2048 if cache_mode[0]==2 else (slot[0] if slot[0] is not None else pattern)
 if cache_mode[0]==3:epoch.value+=1
 uploaded[out[0]]=c.string_at(source,32);return 1
binding=Binding()
def admit():
 assert activate(c.byref(lease),c.byref(owner),c.byref(epoch),read,None,cram)==1
 assert bind(c.byref(binding),c.byref(plan),c.byref(lease),acquire,None)==1
admit()
with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=assemble_region(z)
pixel_checks=0;opaque_checks=0
for e in entries:
 original=decode_tile(gfx,e.code,8,e.layout);m=maps[e.map]
 for flip in range(4):
  result=c.c_uint16(0x55aa);status=resolve(c.byref(binding),e.code,e.group|(flip<<5),e.layout,0x8000,c.byref(result))
  if not e.pattern[flip]:assert status==2 and result.value==0x55aa;continue
  assert status==1 and (result.value>>13)&3==m.page and not result.value&0x1800 and result.value&0x8000
  packed=uploaded[result.value&0x7ff]
  for y in range(8):
   for x in range(8):
    pen=original[7-y if flip&2 else y][7-x if flip&1 else x];byte=packed[y*4+x//2];mapped=(byte>>(0 if x&1 else 4))&15
    assert mapped==(0 if pen==15 else m.pens[pen+1]);assert (mapped==0)==(pen==15)
    if mapped:opaque_checks+=1
    pixel_checks+=1

# Ownership/refusal cases cannot publish a partially valid visual frame.
frame=Frame();assert begin(c.byref(frame),c.byref(lease))==1 and not publish(c.byref(frame))
assert prepared(c.byref(frame),1)==1 and prepared(c.byref(frame),2)==1 and not publish(c.byref(frame))
assert prepared(c.byref(frame),8)==0 and prepared(c.byref(frame),4)==1 and publish(c.byref(frame))==1
epoch.value+=1;assert not publish(c.byref(frame)) and not current(c.byref(lease));admit()
nonblank=next(e for e in entries if e.pattern[0]);blank=next(e for e in entries if not e.pattern[0]);out=c.c_uint16(0x55aa)
for mode in (1,2,3):
 read_mode[0]=mode;assert activate(c.byref(lease),c.byref(owner),c.byref(epoch),read,None,cram)==0 and not lease.plan
read_mode[0]=0;admit()
wrong=(c.c_uint16*64)(*cram);wrong[1]^=2;assert activate(c.byref(lease),c.byref(owner),c.byref(epoch),read,None,wrong)==0;admit()
old=maps[0].pens[1];maps[0].pens[1]=0;assert not valid(c.byref(plan));maps[0].pens[1]=old
old=entries[0].pattern[0];entries[0].pattern[0]=plan.patternCount;assert not valid(c.byref(plan));entries[0].pattern[0]=old
assert resolve(c.byref(binding),0x6000,0,0,0,c.byref(out))==2
assert resolve(c.byref(binding),nonblank.code,30,nonblank.layout,0,c.byref(out))==0
assert resolve(c.byref(binding),nonblank.code,nonblank.group,2,0,c.byref(out))==0
assert resolve(c.byref(binding),nonblank.code,nonblank.group,nonblank.layout,0x2000,c.byref(out))==0
for mode in (1,2,3):
 cache_mode[0]=mode;admit();out.value=0x55aa
 assert resolve(c.byref(binding),nonblank.code,nonblank.group,nonblank.layout,0x8000,c.byref(out))==0 and out.value==0x55aa
cache_mode[0]=0;admit()
# An admitted plan may omit unused future source groups; use must then fail.
saved=owner.count;owner.count=0;admit();assert binding.groups==0
assert resolve(c.byref(binding),blank.code,blank.group,blank.layout,0,c.byref(out))==2
assert resolve(c.byref(binding),nonblank.code,nonblank.group,nonblank.layout,0,c.byref(out))==0
owner.count=saved;admit()

tile_lookup={(e['code'],e['source_group'],e['layout']):e for e in data['entries']}
def text_rows(raw,base):
 rows=[];nonblank=0
 for row in range(2,30):
  line=[]
  for col in range(4,44):
   code,attr=source_cell(raw,col,row,base)
   if not 0x7000<=code<=0x7fff:line.append(None);continue
   e=tile_lookup[code,attr&31,col&1];pattern=e['patterns_by_source_flip'][(attr>>5)&3]
   if not pattern:line.append(None);continue
   m=data['maps'][e['map']];line.append((pattern,m['page'],code,attr,col&1));nonblank+=1
  rows.append(line)
 return rows,nonblank
def plan_rows(rows,first_tile=1,upload=None):
 budget=Budget();pieces=(Piece*80)();count=c.c_uint16();next_tile=first_tile
 for y,line in enumerate(rows):
  attrs=(c.c_uint16*40)();present=(c.c_uint8*5)()
  for x,cell in enumerate(line):
   if cell is None:continue
   pattern,page,code,attr,layout=cell;slot[0]=next_tile;value=c.c_uint16()
   assert resolve(c.byref(binding),code,attr,layout,0x8000,c.byref(value))==1
   assert value.value==(0x8000|(page<<13)|next_tile)
   attrs[x]=value.value;present[x>>3]|=1<<(x&7)
   if upload is not None:upload(next_tile,blob[pattern*32:(pattern+1)*32])
   next_tile+=1
  status=row_plan(attrs,present,y*8,c.byref(budget),pieces,80,c.byref(count))
  if status:
   slot[0]=None
   return status,budget,pieces,count.value,next_tile-first_tile
 slot[0]=None
 return 0,budget,pieces,count.value,next_tile-first_tile
all_text=[];text_budgets={};text_status={}
for capture in data['captures']:
 raw=Path(capture['path']).read_bytes();rows,visible=text_rows(raw,capture['base']);status,budget,pieces,count,staged=plan_rows(rows)
 item={'path':capture['path'],'kind':capture['kind'],'frame':capture['frame'],'status':status,'sat_entries':count,'visible_8x8_cells':visible,'staged_vram_tiles':staged,'max_line_sprites':max(budget.sprites),'max_line_pixels':max(budget.pixels)}
 all_text.append(item);text_budgets[capture['path']]=budget;text_status[capture['path']]=status

object_pixels={}
@Asset
def opaque_object(_,code,attr,x,y,out):
 if code not in object_pixels:object_pixels[code]=decode_tile(gfx,code,16)
 if all(p==15 for row in object_pixels[code] for p in row):return 2
 out[0]=0;return 1 # Budget-only callback: no pixels published by this pass.
combined=[]
for capture in data['captures']:
 if capture['kind']!='intro_vram':continue
 raw=Path(capture['path']).read_bytes()
 for bank in (0,0x8000):
  if text_status[capture['path']]:
   combined.append({'frame':capture['frame'],'bank':bank,'status':text_status[capture['path']],'rejected_participant':'scroll1','object_pass_skipped':True,'publishable':False})
   continue
  original=text_budgets[capture['path']];budget=Budget.from_buffer_copy(bytes(original));words=(c.c_uint16*1024)(*struct.unpack('>1024H',raw[bank:bank+0x800]));pieces=(ObjectPiece*80)();result=Result()
  lib.willowScroll1TestObjects(words,c.byref(budget),pieces,opaque_object,None,c.byref(result))
  combined.append({'frame':capture['frame'],'bank':bank,'status':result.status,'publishable':not result.status,'text_entries':original.count,'object_entries':result.pieces,'total_entries':budget.count,'max_line_sprites':max(budget.sprites),'max_line_pixels':max(budget.pixels)})
# Definite overflow proof: forty one-tile alternating pages require40 objects
# on one line, exceeding H40's20. Entire frame remains unpublishable.
attrs=(c.c_uint16*40)(1,2,3,4);presence=(c.c_uint8*5)(15,0,0,0,0);budget=Budget();pieces=(Piece*80)();count=c.c_uint16()
assert row_plan(attrs,presence,16,c.byref(budget),pieces,80,c.byref(count))==0 and count.value==1 and pieces[0].width==4
assert list(budget.sprites)==[int(16<=y<24) for y in range(224)]
assert list(budget.pixels)==[32 if 16<=y<24 else 0 for y in range(224)]
attrs=(c.c_uint16*40)(*((1+x)|((x&1)<<13) for x in range(40)));presence=(c.c_uint8*5)(*([255]*5));budget=Budget();pieces=(Piece*80)();count=c.c_uint16()
assert row_plan(attrs,presence,0,c.byref(budget),pieces,80,c.byref(count))==4
frame=Frame();assert begin(c.byref(frame),c.byref(lease));assert prepared(c.byref(frame),3);assert not publish(c.byref(frame))

# The original source composition must match the real arcade capture exactly
# before assigning this frame's A/B/SAT priority. No generic priority claim.
proof=runpy.run_path(str(ROOT/'tools/prove_native_opening_frame.py'))
assert json.loads((OUT/'source-composition-1020.json').read_text())['mismatches']==0
bg=json.loads((OUT/'background_reuse_p3_w16.json').read_text());bgblob=(OUT/'background_reuse_p3_w16.bin').read_bytes();bgmap=struct.unpack('>6080H',(OUT/'background_reuse_p3_w16.map16be').read_bytes())
obj=json.loads((OUT/'object_palette_reuse_p3_w16.json').read_text());objblob=(OUT/'object_palette_reuse_p3_w16.bin').read_bytes();objlookup={(e['code'],e['source_group']):e for e in obj['entries']}
# Admit every source group actively consumed by this complete frame against
# its captured CPS palette RAM. Keep both original player groups in the
# contract even though this particular source frame displays only Willow.
active_sources={int(g)*16:words for g,words in obj['source_palettes'].items()}
for group in data['capture_active_palette_groups']['1020']:
 active_sources[0x200+group*16]=data['source_palette_words'][str(group)]
active_bg={2:set(),3:set()}
for layer in (2,3):
 for y in range(224):
  for x in range(320):
   pen,group=proof['layers'].pen(layer,x+32,y-240)
   if pen!=15:active_bg[layer].add(group)
 for group in active_bg[layer]:
  active_sources[layer*0x200+group*16]=bg['source_palette_words'][str(layer)][group]
sources=(Source*len(active_sources))(*(Source(base,(c.c_uint16*16)(*words)) for base,words in sorted(active_sources.items())))
owner=Owner(cram,sources,len(sources),7);epoch.value+=1;captured_reads=[0]
@Read
def captured_read(_,index,out):
 at=0x1c000+index*2
 out[0]=int.from_bytes(proof['ram'][at:at+2],'big');captured_reads[0]+=1;return 1
assert activate(c.byref(lease),c.byref(owner),c.byref(epoch),captured_read,None,cram)
assert bind(c.byref(binding),c.byref(plan),c.byref(lease),acquire,None)
complete_frame=Frame();assert begin(c.byref(complete_frame),c.byref(lease))
vram=bytearray(65536);resident_bg={0:0};resident_obj={};next_tile=16;ownership=[]
def upload(tile,packed):
 assert tile*32+len(packed)<=0xc000,'Pattern allocation overlaps PlaneA'
 vram[tile*32:tile*32+len(packed)]=packed
def allocate_bg(pattern):
 global next_tile
 if pattern not in resident_bg:resident_bg[pattern]=next_tile;upload(next_tile,bgblob[pattern*32:(pattern+1)*32]);next_tile+=1
 return resident_bg[pattern]
for base,start,table in ((0,64,0xc000),(3680,32,0xe000)):
 for y in range(28):
  for x in range(40):
   value=bgmap[base+(start+y)*40+x];page=value>>14;pattern=value&0x3fff;tile=allocate_bg(pattern)
   struct.pack_into('>H',vram,table+(y*64+x)*2,(page<<13)|tile)
ownership.append({'kind':'background','first_tile':16,'tiles':next_tile-16,'unique_source_patterns_including_blank':len(resident_bg)})
assert prepared(c.byref(complete_frame),1)
object_start=next_tile
@Asset
def object_asset(_,code,attr,x,y,out):
 global next_tile
 e=objlookup.get((code,attr&31))
 if e is None:return 0
 block=e['variant_block']
 if not block:return 2
 if block not in resident_obj:
  resident_obj[block]=next_tile;upload(next_tile,objblob[block*128:(block+1)*128]);next_tile+=4
 out[0]=0x8000|(obj['maps'][e['map']]['page']<<13)|resident_obj[block];return 1
records=proof['records'];bankwords=[v for record in records for v in record]+[0,0,0,0xff00];bankwords+=[0]*(1024-len(bankwords))
objbudget=Budget();object_pieces=(ObjectPiece*80)();object_result=Result();lib.willowScroll1TestObjects((c.c_uint16*1024)(*bankwords),c.byref(objbudget),object_pieces,object_asset,None,c.byref(object_result));assert not object_result.status
ownership.append({'kind':'objects','first_tile':object_start,'tiles':next_tile-object_start,'unique_16x16_blocks':len(resident_obj)})
text_start=next_tile;rows,visible=text_rows(proof['ram'],0x10000);status,fullbudget,text_pieces,text_count,text_tiles=plan_rows(rows,next_tile,upload);assert not status;next_tile+=text_tiles
ownership.append({'kind':'scroll1_ordered_row_staging','first_tile':text_start,'tiles':text_tiles})
# Reserve text first (frontmost), then re-run source objects against that
# same H40 budget. Cached object tiles retain their source/plan identity.
object_result=Result();lib.willowScroll1TestObjects((c.c_uint16*1024)(*bankwords),c.byref(fullbudget),object_pieces,object_asset,None,c.byref(object_result));assert not object_result.status
assert prepared(c.byref(complete_frame),2) and prepared(c.byref(complete_frame),4) and publish(c.byref(complete_frame))
sat=[]
for n in range(text_count):
 words=(c.c_uint16*4)();assert sat_words(c.byref(text_pieces[n]),0,words);sat.append(list(words))
for n in range(object_result.pieces):
 p=object_pieces[n];sat.append([p.y+128,0x500,p.attribute,p.x+128])
for n,words in enumerate(sat):
 words[1]|=n+1 if n+1<len(sat) else 0;struct.pack_into('>4H',vram,0xf400+n*8,*words)
assert len(sat)<=80 and fullbudget.count==len(sat)
registers=bytearray(24);registers[0]=4;registers[1]=0x54;registers[2]=0x30;registers[3]=0;registers[4]=7;registers[5]=0x7a;registers[7]=0;registers[10]=0xff;registers[12]=0x81;registers[13]=0x3c;registers[15]=2;registers[16]=1
buffers={'vram.bin':bytes(vram),'cram.bin':struct.pack('>64H',*cram),'vsram.bin':bytes(80),'registers.bin':bytes(registers)}
dest=OUT/'frame1020_reuse_p3_w16_v1';dest.mkdir(exist_ok=True)
for name,content in buffers.items():
 if (dest/name).exists():assert (dest/name).read_bytes()==content,'Frozen frame1020 hardware buffers changed'
 else:(dest/name).write_bytes(content)
(dest/'shared-ownership.json').write_text(json.dumps({'source_capture':'build/arcade-trace/intro-vram-1020.bin','source_palette_word_origin':'0x91c000','source_blocks':[{'base_word':base,'words':words} for base,words in sorted(active_sources.items())],'captured_words_verified':captured_reads[0],'background_active_groups':active_bg,'required_participants':7,'prepared_participants':complete_frame.ready,'publishable':bool(publish(c.byref(complete_frame))),'cram_sha256':hashlib.sha256(buffers['cram.bin']).hexdigest(),'scope':'Static late-opening frame1020 admission only. Earlier initialization/fades and later scene palettes require independently admitted plans.'},indent=2,default=lambda value:sorted(value))+'\n')
# Independently decode final VDP words, packed VRAM pixels and SAT order.
def tile_pen(tile,x,y):
 b=vram[tile*32+y*4+x//2];return (b>>(0 if x&1 else 4))&15
def color(word):return tuple(((word>>shift)&7)*34 for shift in (1,5,9))
expected=Image.new('RGB',(320,224),color(cram[0]));sprite_written=set()
for y in range(224):
 for x in range(320):
  for table in (0xe000,0xc000):
   a=int.from_bytes(vram[table+((y//8)*64+x//8)*2:table+((y//8)*64+x//8)*2+2],'big');pen=tile_pen(a&0x7ff,x&7,y&7)
   if pen:expected.putpixel((x,y),color(cram[((a>>13)&3)*16+pen]))
for yw,size,attr,xw in sat:
 width=((size>>10)&3)+1;height=((size>>8)&3)+1;x=xw-128;y=yw-128
 for yy in range(height*8):
  sy=height*8-1-yy if attr&0x1000 else yy
  for xx in range(width*8):
   px=x+xx;py=y+yy
   if not(0<=px<320 and 0<=py<224) or (px,py) in sprite_written:continue
   sx=width*8-1-xx if attr&0x800 else xx;tile=(attr&0x7ff)+(sx//8)*height+sy//8;pen=tile_pen(tile,sx&7,sy&7)
   if pen:expected.putpixel((px,py),color(cram[((attr>>13)&3)*16+pen]));sprite_written.add((px,py))
expected.save(dest/'expected-rgb888.png')
frame_report={'frame':1020,'source_actor_frame':1018,'source_crop':[32,0,352,224],'source_composition_pixels_verified':71680,'source_composition_mismatches':0,'source_priority_for_this_frame':'Scroll1 > front-first source objects > Scroll2 > Scroll3; exact captured-source pixel proof. Genesis text/objects high priority; A/B low priority. No global layer-priority claim.','source_object_records':records,'source_scroll1':'intro-vram-1020.bin, Scroll1 offset10000; source columns4..43 rows2..29, unchanged8x8 geometry/layout/flips.','background':'ROM SourceLayers camera0; near source y-240 maps row64, far y-240 maps row32. Plane tilemap rows rebased to0 and VSRAM0 for this static fixture.','ownership':ownership,'highest_pattern_tile':next_tile-1,'pattern_ceiling_tile':1535,'plane_a':[0xc000,0xcfff],'plane_b':[0xe000,0xefff],'sat':[0xf400,0xf67f],'hscroll':[0xf000,0xf003],'palette':'One immutable64-word reuse_p3_w16 plan; no font/overlay/HUD PAL0 replacement. Both player pages byte-identical.','sat_entries':len(sat),'text_sat_entries':text_count,'object_sat_entries':object_result.pieces,'max_line_sprites':max(fullbudget.sprites),'max_line_pixels':max(fullbudget.pixels),'buffers':{name:{'bytes':len(content),'sha256':hashlib.sha256(content).hexdigest()} for name,content in buffers.items()},'expected_image':'expected-rgb888.png','scope':'Host-verified full VRAM/CRAM/VSRAM/register fixture for independent native68000 VDP capture. Not yet actual emulator pixels or production gameplay.'}
(dest/'manifest.json').write_text(json.dumps(frame_report,indent=2)+'\n')
evidence={'source_oriented_pixels_through_C_lookup':pixel_checks,'opaque_pixels_through_C_lookup':opaque_checks,'generated_oriented_source_pixels_including_blank':data['verified_oriented_pixels'],'font_overlay_hud_code_group_keys':data['source_code_groups'],'pattern_bytes':len(blob),'text_capture_cases':len(all_text),'text_failures':[r for r in all_text if r['status']],'text_accepted_cases':sum(not r['status'] for r in all_text),'text_peak_sat':max(r['sat_entries'] for r in all_text if not r['status']),'text_peak_vram_tiles':max(r['staged_vram_tiles'] for r in all_text if not r['status']),'text_peak_line_sprites':max(r['max_line_sprites'] for r in all_text if not r['status']),'text_peak_line_pixels':max(r['max_line_pixels'] for r in all_text if not r['status']),'combined_capture_cases':len(combined),'combined_failures':[r for r in combined if r['status']],'combined_peak_sat':max(r.get('total_entries',0) for r in combined),'combined_peak_line_sprites':max(r.get('max_line_sprites',0) for r in combined),'combined_peak_line_pixels':max(r.get('max_line_pixels',0) for r in combined),'combined':combined,'shared_epoch_tests':'Exact active source words/CRAM, unadmitted group refusal, stale epoch, missing participant, invalid participant, cache failure, invalid physical tile, stale cache callback, opaque/transparent continuity. Alternating-page40-cell row rejects H40 limit and cannot publish.','complete_frame':str(dest/'manifest.json'),'scope':'Original geometry320px source crop. Existing384-to320 HUD resampling path is NOT silently reused; that composition would require fitting each composite cell against the same shared plan. Catalog is current enumerated/captured source UI, not all game scripts.'}
(OUT/'scroll1-presentation-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps({k:v for k,v in evidence.items() if k not in ('combined','text_failures','combined_failures')},indent=2));print(json.dumps(frame_report,indent=2))
