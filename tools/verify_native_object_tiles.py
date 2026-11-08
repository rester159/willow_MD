#!/usr/bin/env python3
"""Verify generated source-pen atlas through its C callback and encoded SAT.

The isolated object-plane test assigns actual source groups separate symbolic
CRAM pages. It does NOT assign game RGB values or solve full-frame CRAM fit.
"""
from pathlib import Path
import ctypes as c
import hashlib
import json
import struct
import subprocess
import zipfile
import native_object_tiles as packing
# Reuse the already independent CPS pixel reference and ABI declarations;
# importing also reruns its broader hardware-geometry regression suite.
import verify_native_object_sat_bridge as geometry

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit'
report=json.loads((OUT/'opening_object_tiles.json').read_text())
blob=(OUT/'opening_object_tiles.bin').read_bytes()
assert hashlib.sha256(blob).hexdigest()==report['binary_sha256']
class Entry(c.Structure):_fields_=[('code',c.c_uint16),('block',c.c_uint16)]
class Atlas(c.Structure):
 _fields_=[('entries',c.POINTER(Entry)),('patterns',c.POINTER(c.c_uint8)),('bytes',c.c_uint32),('count',c.c_uint16),('blocks',c.c_uint16)]
Presentation=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_int16,c.c_int16,c.POINTER(c.c_uint16))
Acquire=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.POINTER(Atlas),c.c_uint16,c.POINTER(c.c_uint8),c.POINTER(c.c_uint16))
class Binding(c.Structure):
 _fields_=[('atlas',c.POINTER(Atlas)),('presentation',Presentation),('presentationContext',c.c_void_p),('acquire',Acquire),('cacheContext',c.c_void_p)]
entries=(Entry*len(report['entries']))(*(Entry(*e) for e in report['entries']))
patterns=(c.c_uint8*len(blob)).from_buffer_copy(blob)
atlas=Atlas(entries,patterns,len(blob),len(entries),len(blob)//128)
libpath=OUT/'object-tiles-test.dylib'
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',str(ROOT/'tools/test_native_object_tiles.c'),'-o',str(libpath)],check=True)
lib=c.CDLL(str(libpath))
lib.willowObjectTilesTestValid.argtypes=[c.POINTER(Atlas)];lib.willowObjectTilesTestValid.restype=c.c_uint
lib.willowObjectTilesTestAsset.argtypes=[c.POINTER(Binding),c.c_uint16,c.c_uint16,c.POINTER(c.c_uint16)];lib.willowObjectTilesTestAsset.restype=c.c_uint
lib.willowObjectTilesTestBuild.argtypes=[c.POINTER(c.c_uint16),c.POINTER(geometry.View),c.POINTER(geometry.Budget),c.POINTER(geometry.Piece),c.POINTER(Binding),c.POINTER(geometry.Result),c.POINTER(c.c_uint16)]
assert lib.willowObjectTilesTestValid(c.byref(atlas))==1
original_bytes=atlas.bytes;atlas.bytes-=1;assert lib.willowObjectTilesTestValid(c.byref(atlas))==0;atlas.bytes=original_bytes
patterns[0]=1;assert lib.willowObjectTilesTestValid(c.byref(atlas))==0;patterns[0]=0
old=entries[0].block;entries[0].block=atlas.blocks;assert lib.willowObjectTilesTestValid(c.byref(atlas))==0;entries[0].block=old
old=entries[1].code;entries[1].code=entries[0].code;assert lib.willowObjectTilesTestValid(c.byref(atlas))==0;entries[1].code=old

frames=[];pixel_checks=0;all_calls=0;all_uploads=0
nonblank=next(entry.code for entry in entries if entry.block)
test_cases=list(report['captures'])
for flip in (0,1):
 test_cases.append({'path':'synthetic duplicate code, distinct source palettes',
  'bank':0,'global_flip':flip,'unique_nonblank_blocks':1,
  'words':[104,40,nonblank,0,140,40,nonblank,1,0,0,0,0xff00]+[0]*1012})
for capture in test_cases:
 if 'words' in capture:words=capture['words']
 else:
  raw=Path(capture['path']).read_bytes();words=struct.unpack('>1024H',raw[capture['bank']:capture['bank']+0x800])
 v=geometry.View(96,16,320,224,capture['global_flip']);budget=geometry.Budget();pieces=(geometry.Piece*80)();result=geometry.Result();sat=(c.c_uint16*320)()
 pages={};resident={};vram={};calls=[];callback_errors=[]
 @Presentation
 def presentation(_,code,attribute,x,y,out):
  pal=attribute&31
  if pal not in pages:
   if len(pages)==4:return 0
   pages[pal]=len(pages)
  out[0]=pages[pal]<<13;return 1
 @Acquire
 def acquire(_,descriptor,block,source,out):
  # Test-only resident cache; identity/bytes are checked before admitting DMA.
  if c.addressof(descriptor.contents)!=c.addressof(atlas):
   callback_errors.append('wrong atlas identity');return 0
  chunk=c.string_at(source,128)
  if chunk!=blob[block*128:(block+1)*128]:
   callback_errors.append('wrong pattern bytes');return 0
  calls.append(block)
  if block not in resident:
   if len(resident)==80:return 0
   resident[block]=len(resident)*4
   vram[resident[block]]=chunk
  out[0]=resident[block];return 1
 binding=Binding(c.pointer(atlas),presentation,None,acquire,None)
 array=(c.c_uint16*1024)(*words)
 lib.willowObjectTilesTestBuild(array,c.byref(v),c.byref(budget),pieces,c.byref(binding),c.byref(result),sat)
 assert result.status==0,(capture,result.status)
 assert not callback_errors,callback_errors
 assert list(array)==list(words)
 image=[-1]*(320*224);reverse={page:pal for pal,page in pages.items()}
 for index in range(result.pieces):
  yw,szlink,attr,xw=sat[index*4:index*4+4]
  assert szlink>>8==5 and szlink&255==(index+1 if index+1<result.pieces else 0)
  chunk=vram[attr&0x7ff];pal=reverse[(attr>>13)&3];x=xw-128;y=yw-128
  for yy in range(16):
   py=y+yy
   if not 0<=py<224:continue
   sy=15-yy if attr&0x1000 else yy
   for xx in range(16):
    px=x+xx
    if not 0<=px<320:continue
    sx=15-xx if attr&0x800 else xx
    offset=((sx//8)*2+sy//8)*32+(sy&7)*4+(sx&7)//2
    pen=(chunk[offset]>>(0 if sx&1 else 4))&15
    pos=py*320+px
    if pen and image[pos]<0:image[pos]=pal*16+pen-1
 assert image==geometry.reference(words,v),capture
 assert len(resident)==capture['unique_nonblank_blocks']
 frames.append({'file':capture['path'],'bank':capture['bank'],'flip':capture['global_flip'],'pieces':result.pieces,'cache_requests':len(calls),'unique_uploads':len(resident),'dma_bytes':len(resident)*128,'symbolic_object_cram_pages':len(pages)})
 pixel_checks+=320*224;all_calls+=len(calls);all_uploads+=len(resident)

# Refusal must preserve caller output; transparency must not acquire/fix colors.
nonblank=next(entry.code for entry in entries if entry.block)
blank=next(entry.code for entry in entries if not entry.block)
counts=[0,0];mode=[0]
@Presentation
def reject_presentation(_,code,source,x,y,out):
 counts[0]+=1
 if mode[0]==1:return 0
 out[0]=0x0800 if mode[0]==2 else 0xa000
 return 1
@Acquire
def reject_cache(_,descriptor,block,source,out):
 counts[1]+=1
 if mode[0]==3:return 0
 out[0]=2045 if mode[0]==4 else 7
 return 1
binding=Binding(c.pointer(atlas),reject_presentation,None,reject_cache,None)
value=c.c_uint16(0x55aa)
assert lib.willowObjectTilesTestAsset(c.byref(binding),blank,0,c.byref(value))==2 and counts==[0,0] and value.value==0x55aa
assert lib.willowObjectTilesTestAsset(c.byref(binding),0xffff,0,c.byref(value))==0 and counts==[0,0]
for bad in range(1,5):
 mode[0]=bad;value.value=0x55aa
 assert lib.willowObjectTilesTestAsset(c.byref(binding),nonblank,0,c.byref(value))==0 and value.value==0x55aa
mode[0]=0
assert lib.willowObjectTilesTestAsset(c.byref(binding),nonblank,0,c.byref(value))==1 and value.value==0xa007
# General arbitrary-code path, independent of the opening capture selection.
with zipfile.ZipFile(ROOT/'willow.zip') as z:gfx=packing.assemble_region(z)
general_codes=[0,1,15,16,0x27ff,0x2800,0x28ef,0x7fff]
general,packed=packing.pack(gfx,general_codes)
assert packing.verify(gfx,general,packed)==2048
evidence={'captured_bank_flip_cases':len(report['captures']),'duplicate_code_cache_cases':2,'encoded_sat_pixels_compared':pixel_checks,'source_catalog_pixels_compared':report['verified_pixels'],'general_source_pixels_compared':2048,'cache_requests':all_calls,'unique_frame_uploads':all_uploads,'frames':frames,'catalog_sha256':report['binary_sha256'],'scope':'Generated unflipped ROM pen atlas and compiled host lookup/callback consumer. Palette pages are symbolic object-only identities; production palette fitting, tile ownership/publication timing, real DMA and headless cartridge admission remain open.'}
(OUT/'object-tiles-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
print({k:v for k,v in evidence.items() if k!='frames'})
