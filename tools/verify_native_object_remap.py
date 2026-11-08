#!/usr/bin/env python3
"""Check explicit palette plans against ROM pixels and encoded VDP SAT words.

This is an isolated object-plane host check. It neither changes production
palettes nor establishes actual cartridge DMA, priority or frame admission.
"""
from pathlib import Path
import ctypes as c
import hashlib,json,struct,subprocess
import native_object_remap as packing
import verify_native_object_sat_bridge as geometry
from cps1_palette import cps_rgb
from native_palette_fit import color_error

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit'
catalog=json.loads((OUT/'opening_object_tiles.json').read_text());raw=(OUT/'opening_object_tiles.bin').read_bytes()
source_words=json.loads((OUT/'object-palette-fit.json').read_text())['source_palette_words']
class Entry(c.Structure):_fields_=[('code',c.c_uint16),('block',c.c_uint16)]
class Atlas(c.Structure):_fields_=[('entries',c.POINTER(Entry)),('patterns',c.POINTER(c.c_uint8)),('bytes',c.c_uint32),('count',c.c_uint16),('blocks',c.c_uint16)]
class Palette(c.Structure):_fields_=[('words',c.c_uint16*16),('group',c.c_uint16)]
class Map(c.Structure):_fields_=[('palette',c.c_uint16),('page',c.c_uint16),('pens',c.c_uint8*16)]
class Variant(c.Structure):_fields_=[('code',c.c_uint16),('sourceBlock',c.c_uint16),('block',c.c_uint16),('map',c.c_uint16)]
class Plan(c.Structure):_fields_=[('source',c.POINTER(Atlas)),('variants',c.POINTER(Atlas)),('entries',c.POINTER(Variant)),('maps',c.POINTER(Map)),('palettes',c.POINTER(Palette)),('cram',c.POINTER(c.c_uint16)),('count',c.c_uint16),('mapCount',c.c_uint16),('paletteCount',c.c_uint16)]
class Lease(c.Structure):_fields_=[('plan',c.POINTER(Plan)),('epoch',c.POINTER(c.c_uint32)),('generation',c.c_uint32)]
Presentation=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_int16,c.c_int16,c.POINTER(c.c_uint16))
Acquire=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.POINTER(Atlas),c.c_uint16,c.POINTER(c.c_uint8),c.POINTER(c.c_uint16))
Read=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.POINTER(c.c_uint16))
class Binding(c.Structure):_fields_=[('lease',c.POINTER(Lease)),('priority',Presentation),('priorityContext',c.c_void_p),('acquire',Acquire),('cacheContext',c.c_void_p)]
raw_entries=(Entry*len(catalog['entries']))(*(Entry(*e) for e in catalog['entries']));raw_patterns=(c.c_uint8*len(raw)).from_buffer_copy(raw)
atlas=Atlas(raw_entries,raw_patterns,len(raw),len(raw_entries),len(raw)//128)
libpath=OUT/'object-remap-test.dylib'
subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-O2','-shared','-fPIC',str(ROOT/'tools/test_native_object_remap.c'),'-o',str(libpath)],check=True)
lib=c.CDLL(str(libpath))
lib.willowRemapTestValid.argtypes=[c.POINTER(Plan)];lib.willowRemapTestValid.restype=c.c_uint
lib.willowRemapTestBake.argtypes=[c.POINTER(c.c_uint8),c.POINTER(c.c_uint8),c.POINTER(c.c_uint8)];lib.willowRemapTestBake.restype=c.c_uint
lib.willowRemapTestActivate.argtypes=[c.POINTER(Lease),c.POINTER(Plan),c.POINTER(c.c_uint32),Read,c.c_void_p,c.POINTER(c.c_uint16)];lib.willowRemapTestActivate.restype=c.c_uint
lib.willowRemapTestCurrent.argtypes=[c.POINTER(Lease)];lib.willowRemapTestCurrent.restype=c.c_uint
lib.willowRemapTestAsset.argtypes=[c.POINTER(Binding),c.c_uint16,c.c_uint16,c.POINTER(c.c_uint16)];lib.willowRemapTestAsset.restype=c.c_uint
lib.willowRemapTestBuild.argtypes=[c.POINTER(c.c_uint16),c.POINTER(geometry.View),c.POINTER(geometry.Budget),c.POINTER(geometry.Piece),c.POINTER(Binding),c.POINTER(geometry.Result),c.POINTER(c.c_uint16)]
read_mode=[0];epoch=c.c_uint32(1)
@Read
def read(_,group,pen,out):
 if read_mode[0]==1:return 0
 out[0]=source_words[str(group)][pen]^(1 if read_mode[0]==2 and group==1 and pen==0 else 0)
 if read_mode[0]==3:epoch.value+=1
 return 1

def source_reference(words,flip,plan):
 """Pixel-first CPS interpretation; no native SAT geometry or atlas decoding."""
 image=[-1]*(320*224)
 mappings={(e['code'],e['source_group']):plan['maps'][e['map']] for e in plan['entries']}
 for at in range(0,len(words),4):
  x,y,code,a=words[at:at+4]
  if a&0xff00==0xff00:break
  if code>0x27ff:continue
  w=(((a>>8)&15)+1)*16;h=((a>>12)+1)*16
  for yy in range(h):
   rawy=((y+(yy//16)*16)&511)+(yy&15);py=(255-rawy if flip else rawy)-16
   if not 0<=py<224:continue
   sy=h-1-yy if a&64 else yy
   for xx in range(w):
    rawx=((x+(xx//16)*16)&511)+(xx&15);px=(511-rawx if flip else rawx)-96
    if not 0<=px<320:continue
    sx=w-1-xx if a&32 else xx;child=(code&~15)+((code+sx//16)&15)+(sy//16)*16
    pen=geometry.tile(child)[sy&15][sx&15];pos=py*320+px
    if pen!=15 and image[pos]<0:
     m=mappings[child,a&31];image[pos]=plan['cram'][m['page']][m['pens'][pen+1]]
 return image

reports=[];pixel_checks=0;pattern_checks=0;anchor_checks=0
reserved=json.loads((OUT/'object_palette_unchanged_background.json').read_text())['cram'][:2]
for path in sorted(OUT.glob('object_palette_*.json')):
 data=json.loads(path.read_text());palette_data,map_data,entry_data,blob=packing.prepare(data,catalog,raw)
 assert data['cram'][:2]==reserved,'A reserved player page changed'
 anchors={(a['group'],a['source_pen']):a for a in data.get('anchors',[])}
 palettes=(Palette*len(palette_data))(*(Palette((c.c_uint16*16)(*w),g) for g,w in palette_data))
 maps=(Map*len(map_data))(*(Map(g,p,(c.c_uint8*16)(*pens)) for g,p,pens in map_data))
 entries=(Variant*len(entry_data))(*(Variant(*e) for e in entry_data))
 patterns=(c.c_uint8*len(blob)).from_buffer_copy(blob);variants=Atlas(None,patterns,len(blob),0,len(blob)//128)
 cram=(c.c_uint16*64)(*(v for page in data['cram'] for v in page))
 plan=Plan(c.pointer(atlas),c.pointer(variants),entries,maps,palettes,cram,len(entries),len(maps),len(palettes))
 assert lib.willowRemapTestValid(c.byref(plan))==1
 # Each packed variant pixel is independently checked against the original
 # CPS decoder, including visible black and all transparent pixels.
 color_sums={};opaque=0
 for e in entries:
  m=maps[e.map];group=palettes[m.palette].group;original=geometry.tile(e.code)
  source_rgb=[cps_rgb(w) for w in source_words[str(group)]]
  for y in range(16):
   for x in range(16):
    pen=original[y][x];off=e.block*128+((x//8)*2+y//8)*32+(y&7)*4+(x&7)//2
    mapped=(blob[off]>>(0 if x&1 else 4))&15
    assert mapped==(0 if pen==15 else m.pens[pen+1]);assert (mapped==0)==(pen==15)
    if mapped:
     word=cram[m.page*16+mapped];rgb=tuple(((word>>s)&7)*34 for s in (1,5,9));src=source_rgb[pen]
     if (group,pen) in anchors:
      a=anchors[group,pen];assert source_words[str(group)][pen]==a['source_word'] and list(src)==a['source_rgb']
      assert rgb==tuple(a['hardware_rgb']) and m.page==data['anchor_page']
      assert (rgb[0]>rgb[2]>rgb[1]) if a['constraint']=='robe' else (rgb[0]>rgb[1]>rgb[2])
      anchor_checks+=1
     sums=color_sums.setdefault(group,[0,0,0]);sums[0]+=1;sums[1]+=sum((a-b)**2 for a,b in zip(rgb,src));sums[2]+=color_error(src,rgb);opaque+=1
    pattern_checks+=1
  out=(c.c_uint8*128)();source=c.cast(c.byref(raw_patterns,e.sourceBlock*128),c.POINTER(c.c_uint8))
  assert lib.willowRemapTestBake(source,m.pens,out)==1 and bytes(out)==blob[e.block*128:(e.block+1)*128]
  inplace=(c.c_uint8*128).from_buffer_copy(bytes(source[:128]));assert lib.willowRemapTestBake(inplace,m.pens,inplace)==1 and bytes(inplace)==bytes(out)
 # Invalid metadata and transparent/opaque violations must be rejected.
 old=maps[0].pens[1];maps[0].pens[1]=0;assert lib.willowRemapTestValid(c.byref(plan))==0
 sentinel=(c.c_uint8*128)(*([0xaa]*128));assert lib.willowRemapTestBake(raw_patterns,maps[0].pens,sentinel)==0 and bytes(sentinel)==bytes([0xaa]*128);maps[0].pens[1]=old
 old=maps[0].page;maps[0].page=4;assert lib.willowRemapTestValid(c.byref(plan))==0;maps[0].page=old
 old=entries[1].sourceBlock;entries[1].sourceBlock=0;assert lib.willowRemapTestValid(c.byref(plan))==0;entries[1].sourceBlock=old
 patterns[128]^=1;assert lib.willowRemapTestValid(c.byref(plan))==0;patterns[128]^=1
 old=cram[0];cram[0]=1;assert lib.willowRemapTestValid(c.byref(plan))==0;cram[0]=old
 old=palettes[1].group;palettes[1].group=palettes[0].group;assert lib.willowRemapTestValid(c.byref(plan))==0;palettes[1].group=old
 lease=Lease()
 def activate():return lib.willowRemapTestActivate(c.byref(lease),c.byref(plan),c.byref(epoch),read,None,cram)
 assert activate()==1
 # Page1 is reserved even though these captures contain no second player.
 for mode in (1,2,3):
  read_mode[0]=mode;assert activate()==0 and not lease.plan
 read_mode[0]=0;assert activate()==1
 changed=(c.c_uint16*64)(*cram);changed[63]^=2
 assert lib.willowRemapTestActivate(c.byref(lease),c.byref(plan),c.byref(epoch),read,None,changed)==0
 assert activate()==1;epoch.value+=1;assert lib.willowRemapTestCurrent(c.byref(lease))==0;assert activate()==1
 frame_records=[];calls=[];resident={};vram={};errors=[];mode=[0]
 @Presentation
 def priority(_,code,attr,x,y,out):
  if mode[0]==1:return 0
  out[0]=0x2000 if mode[0]==2 else 0x8000
  return 1
 @Acquire
 def acquire(_,descriptor,block,source,out):
  if c.addressof(descriptor.contents)!=c.addressof(variants):errors.append('wrong atlas identity');return 0
  if c.string_at(source,128)!=blob[block*128:(block+1)*128]:errors.append('wrong bytes');return 0
  calls.append(block)
  if mode[0]==3:return 0
  if mode[0]==5:epoch.value+=1
  if block not in resident:
   resident[block]=7+len(resident)*4;vram[resident[block]]=bytes(source[:128])
  out[0]=2045 if mode[0]==4 else resident[block];return 1
 binding=Binding(c.pointer(lease),priority,None,acquire,None)
 blank=next(e for e in entries if not e.block);nonblank=next(e for e in entries if e.block);out=c.c_uint16(0x55aa)
 group=lambda e:palettes[maps[e.map].palette].group
 assert lib.willowRemapTestAsset(c.byref(binding),blank.code,group(blank),c.byref(out))==2 and out.value==0x55aa and not calls
 assert lib.willowRemapTestAsset(c.byref(binding),0xffff,0,c.byref(out))==0 and not calls
 assert lib.willowRemapTestAsset(c.byref(binding),nonblank.code,30,c.byref(out))==0 and not calls
 for bad in (1,2,3,4,5):
  mode[0]=bad;assert activate()==1;out.value=0x55aa
  assert lib.willowRemapTestAsset(c.byref(binding),nonblank.code,group(nonblank),c.byref(out))==0 and out.value==0x55aa
 mode[0]=0;assert activate()==1
 for capture in catalog['captures']:
  dump=Path(capture['path']).read_bytes();words=struct.unpack('>1024H',dump[capture['bank']:capture['bank']+0x800]);array=(c.c_uint16*1024)(*words)
  v=geometry.View(96,16,320,224,capture['global_flip']);budget=geometry.Budget();pieces=(geometry.Piece*80)();result=geometry.Result();sat=(c.c_uint16*320)()
  calls.clear();resident.clear();vram.clear()
  lib.willowRemapTestBuild(array,c.byref(v),c.byref(budget),pieces,c.byref(binding),c.byref(result),sat)
  assert result.status==0 and not errors and list(array)==list(words),(path,capture,result.status,errors)
  image=[-1]*(320*224)
  for n in range(result.pieces):
   yw,szlink,attr,xw=sat[n*4:n*4+4];assert szlink>>8==5 and szlink&255==(n+1 if n+1<result.pieces else 0)
   chunk=vram[attr&0x7ff];page=(attr>>13)&3;x=xw-128;y=yw-128;assert attr&0x8000
   for yy in range(16):
    py=y+yy
    if not 0<=py<224:continue
    sy=15-yy if attr&0x1000 else yy
    for xx in range(16):
     px=x+xx
     if not 0<=px<320:continue
     sx=15-xx if attr&0x800 else xx;off=((sx//8)*2+sy//8)*32+(sy&7)*4+(sx&7)//2
     pen=(chunk[off]>>(0 if sx&1 else 4))&15;pos=py*320+px
     if pen and image[pos]<0:image[pos]=cram[page*16+pen]
  assert image==source_reference(words,v.flip,data),(path,capture)
  pixel_checks+=320*224
  frame_records.append({'bank':capture['bank'],'flip':capture['global_flip'],'pieces':result.pieces,'unique_variant_blocks':len(resident),'cold_dma_bytes':len(resident)*128})
 reports.append({'name':data['name'],'catalog_pixels':len(entries)*256,'variant_blocks':variants.blocks,'peak_resident_blocks':max(f['unique_variant_blocks'] for f in frame_records),'frames':len(frame_records),'per_group_pixel_error_sums':color_sums,'binary_sha256':hashlib.sha256(blob).hexdigest()})

evidence={'palette_plans':len(reports),'source_pixels_compared':pattern_checks,'anchored_source_pixels_compared':anchor_checks,'encoded_sat_color_pixels_compared':pixel_checks,'captured_cases_per_plan':len(catalog['captures']),'tests':'ROM pixel identity, declared pen maps, opaque continuity, exact CRAM color reconstruction, source-word/hue anchors, unchanged reserved player pages, required unused player palette, activation source/CRAM/epoch rejection, late cache epoch invalidation, missing code/group refusal, independent encoded SAT geometry','plans':reports,'scope':'Host C callback and immutable resource validation. Four-page palette tradeoffs are explicit; no production DMA/CRAM/layer-priority/timing admission, no full-game asset coverage.'}
(OUT/'object-remap-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(evidence,indent=2))
