#!/usr/bin/env python3
"""Native Window admission against captured CPS state and the frozen1200 VDP map."""
from pathlib import Path
import ctypes as c,hashlib,itertools,json,runpy,struct,subprocess,sys
from cps1_gfx import decode_tile
from native_scroll1_palette_plan import source_cell
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/cartridge-gap-audit';FIX=OUT/'frame1200_window_v1'
subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',str(ROOT/'tools/test_native_window_plan.c'),'-o',str(OUT/'window-test.dylib')],check=True)
class Source(c.Structure):_fields_=[('base',c.c_uint16),('words',c.c_uint16*16)]
class Owner(c.Structure):_fields_=[('cram',c.POINTER(c.c_uint16)),('sources',c.POINTER(Source)),('count',c.c_uint16),('required',c.c_uint16)]
class Lease(c.Structure):_fields_=[('plan',c.POINTER(Owner)),('epoch',c.POINTER(c.c_uint32)),('generation',c.c_uint32)]
class Proof(c.Structure):_fields_=[('epoch',c.POINTER(c.c_uint32)),('generation',c.c_uint32),('left',c.c_uint16),('top',c.c_uint16),('scroll2UnderObjects',c.c_uint16*4)]
class Plan(c.Structure):_fields_=[('lease',c.POINTER(Lease)),('epoch',c.POINTER(c.c_uint32)),('generation',c.c_uint32),('paletteGeneration',c.c_uint32),('scroll1Address',c.c_uint32),('opaqueCells',c.c_uint16),('column',c.c_uint8),('row',c.c_uint8),('firstRow',c.c_uint8),('endRow',c.c_uint8),('reg17',c.c_uint8),('reg18',c.c_uint8),('valid',c.c_uint8)]
PaletteRead=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.POINTER(c.c_uint16));Read=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint32,c.POINTER(c.c_uint16));Asset=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16,c.c_uint8,c.POINTER(c.c_uint16));Stage=c.CFUNCTYPE(c.c_uint8,c.c_void_p,c.c_uint16,c.c_uint16)
lib=c.CDLL(str(OUT/'window-test.dylib'))
def sig(name,args):f=getattr(lib,name);f.argtypes=args;f.restype=c.c_uint;return f
activate=sig('willowWindowTestActivate',[c.POINTER(Lease),c.POINTER(Owner),c.POINTER(c.c_uint32),PaletteRead,c.c_void_p,c.POINTER(c.c_uint16)])
build=sig('willowWindowTestBuild',[c.POINTER(Plan),c.POINTER(c.c_uint16),c.POINTER(c.c_uint16),c.POINTER(Lease),c.POINTER(Proof),Read,c.c_void_p,Asset,c.c_void_p])
prepare=sig('willowWindowTestPrepare',[c.POINTER(Plan),Read,c.c_void_p,Asset,c.c_void_p,Stage,c.c_void_p]);current=sig('willowWindowTestCurrent',[c.POINTER(Plan)]);priority=sig('willowWindowTestPriority',[c.POINTER(Plan),c.POINTER(c.c_uint16)])
p=runpy.run_path(str(ROOT/'tools/prove_native_opening_frame.py'),init_globals={'FRAME':1200,'SOURCE_BANK':0});ram=bytearray(p['ram']);original=bytes(ram);vram=(FIX/'vram.bin').read_bytes();a=(c.c_uint16*32)(*struct.unpack('>32H',(ROOT/'build/arcade-trace/intro-_cps_a_regs-1200.bin').read_bytes()));b=(c.c_uint16*32)(*struct.unpack('>32H',(ROOT/'build/arcade-trace/intro-_cps_b_regs-1200.bin').read_bytes()));source_epoch=c.c_uint32(1);palette_epoch=c.c_uint32(1)
# Build complete original Scroll2-pen usage under actual opaque source objects.
usage=[0]*4;object_points=set();samples=0
for x,y,code,attr in p['records']:
 w=((attr>>8)&15)+1;h=(attr>>12)+1
 for cy in range(h):
  for cx in range(w):
   child=(code&~15)+((code+cx)&15)+16*cy;tile=decode_tile(p['g'],child,16)
   for yy in range(16):
    for xx in range(16):
     if tile[yy][xx]==15:continue
     dx=cx*16+xx;dy=cy*16+yy
     if attr&32:dx=w*16-1-dx
     if attr&64:dy=h*16-1-dy
     px=x-96+dx;py=y-16+dy
     if not(0<=px<320 and 0<=py<224):continue
     object_points.add((px,py));samples+=1;pen,_=p['layers'].pen(2,px+32,py-240);_,bgattr=p['layers'].record(2,px+32,py-240)
     if pen!=15:usage[(bgattr>>7)&3]|=1<<pen
masks=[b[23-g] for g in range(4)];assert all(not(m&u) for m,u in zip(masks,usage))
proof=Proof(c.pointer(source_epoch),source_epoch.value,96,16,(c.c_uint16*4)(*usage));cram=(c.c_uint16*64)(*struct.unpack('>64H',(FIX/'cram.bin').read_bytes()))
# Exact active source-word ownership, including both player groups.
obj=json.loads((OUT/'object_palette_reuse_p3_w16.json').read_text());ui=json.loads((OUT/'scroll1_reuse_p3_w16.json').read_text());bg=json.loads((OUT/'background_reuse_p3_w16.json').read_text());owned={int(g)*16:words for g,words in obj['source_palettes'].items()}
for group in ui['capture_active_palette_groups']['1200']:owned[0x200+group*16]=ui['source_palette_words'][str(group)]
for layer in (2,3):
 groups={p['layers'].pen(layer,x+32,y-240)[1] for y in range(224) for x in range(320) if p['layers'].pen(layer,x+32,y-240)[0]!=15}
 for group in groups:owned[layer*0x200+group*16]=bg['source_palette_words'][str(layer)][group]
sources=(Source*len(owned))(*(Source(base,(c.c_uint16*16)(*words)) for base,words in sorted(owned.items())));owner=Owner(cram,sources,len(sources),7);lease=Lease();palreads=[0]
@PaletteRead
def palette_read(_,index,out):out[0]=int.from_bytes(original[0x1c000+index*2:0x1c002+index*2],'big');palreads[0]+=1;return 1
assert activate(c.byref(lease),c.byref(owner),c.byref(palette_epoch),palette_read,None,cram)
read_mode=[0];asset_mode=[0];stage_mode=[0];staged={};read_addresses=set();lookup={};decoded={};asset_calls=[0,0]
for y in range(28):
 for x in range(40):
  code,attr=source_cell(original,x+4,y+2,0x10000);key=code,attr,(x+4)&1;value=int.from_bytes(vram[0xd000+(y*64+x)*2:0xd002+(y*64+x)*2],'big')&0x7fff
  if key in lookup:assert lookup[key]==value
  lookup[key]=value
@Read
def read(_,address,out):
 if read_mode[0]==1:return 0
 if not(0x900000<=address<0x940000) or address&1:return 0
 at=address-0x900000;out[0]=int.from_bytes(ram[at:at+2],'big');read_addresses.add(address)
 if read_mode[0]==2:source_epoch.value+=1
 return 1
@Asset
def asset(_,code,attr,layout,out):
 asset_calls[bool(out)]+=1
 if asset_mode[0]==1:return 0
 if asset_mode[0]==2:return 3
 key=code,layout
 if key not in decoded:decoded[key]=decode_tile(p['g'],code,8,layout)
 if not any(pen!=15 for row in decoded[key] for pen in row):return 2
 if out:
  if asset_mode[0]==3:out[0]=0x8000
  else:out[0]=lookup.get((code,attr,layout),1)
 if asset_mode[0]==4:source_epoch.value+=1
 return 1
@Stage
def stage(_,index,value):
 if stage_mode[0]==1:return 0
 staged[index]=value
 if stage_mode[0]==2:source_epoch.value+=1
 return 1
plan=Plan()
def plan_build():proof.generation=source_epoch.value;return build(c.byref(plan),a,b,c.byref(lease),c.byref(proof),read,None,asset,None)
def map_prepare():staged.clear();return prepare(c.byref(plan),read,None,asset,None,stage,None)
assert plan_build()==0 and current(c.byref(plan));assert (plan.column,plan.row,plan.firstRow,plan.endRow,plan.reg17,plan.reg18,plan.opaqueCells)==(4,2,0,10,0,10,320)
assert map_prepare()==0 and len(staged)==1120
for index,value in staged.items():assert value==int.from_bytes(vram[0xd000+index*2:0xd002+index*2],'big')
value=c.c_uint16(0xffff);assert priority(c.byref(plan),c.byref(value)) and value.value==0
captured_asset_calls=list(asset_calls)
# Every alternative draw-order permutation must refuse before asset use.
control=b[24]
for order in itertools.permutations(range(4)):
 b[24]=(control&~0x3fc0)|sum(layer<<(6+i*2) for i,layer in enumerate(order));status=plan_build();assert status==(0 if order==(3,2,0,1) else 2)
b[24]=control
for flag in (8,16,32):b[24]=control&~flag;assert plan_build()==2
b[24]=control
for group in range(4):
 old=b[23-group];b[23-group]=old|1;oldusage=proof.scroll2UnderObjects[group];proof.scroll2UnderObjects[group]|=1;assert plan_build()==2;b[23-group]=old;proof.scroll2UnderObjects[group]=oldusage
video=a[17]
for value in (video|1,video|0x8000,video&~2,video&~4,video&~8,video|0x4000):a[17]=value;assert plan_build()==3
a[17]=video;a[6]+=1;assert plan_build()==3;a[6]-=1;a[7]+=1;assert plan_build()==3;a[7]-=1
for left,top in ((63,16),(129,16),(96,15)):proof.left=left;proof.top=top;assert plan_build()==3
proof.left=96;proof.top=16
for mode,want in ((1,4),(2,6)):read_mode[0]=mode;assert plan_build()==want and not plan.valid
read_mode[0]=0
for mode,want in ((1,5),(2,5),(4,6)):asset_mode[0]=mode;assert plan_build()==want and not plan.valid
asset_mode[0]=0;assert plan_build()==0
for mode,want in ((1,7),(2,6)):stage_mode[0]=mode;assert plan_build()==0;assert map_prepare()==want
stage_mode[0]=0;assert plan_build()==0;asset_mode[0]=3;assert map_prepare()==5;asset_mode[0]=0
assert plan_build()==0;source_epoch.value+=1;assert not current(c.byref(plan)) and map_prepare()==6
assert plan_build()==0;palette_epoch.value+=1;assert not current(c.byref(plan)) and map_prepare()==6
assert activate(c.byref(lease),c.byref(owner),c.byref(palette_epoch),palette_read,None,cram)
# Actual source geometry chooses top/bottom/full/empty Window dynamically.
glyph=next((key for key in lookup if 0x7000<=key[0]<=0x7fff and any(pen!=15 for row in decode_tile(p['g'],key[0],8,key[2]) for pen in row)));geometry=[]
for rows,expected in (((),(0,0,0)),((25,),(25,28,0x99)),((12,),(0,13,13)),((0,27),(0,28,28))):
 ram[0x10000:0x14000]=bytes(0x4000)
 for y in rows:
  col=4;row=y+2;at=0x10000+(row&31)*4+col*128+((row&32)<<8);struct.pack_into('>HH',ram,at,glyph[0],glyph[1])
 source_epoch.value+=1;assert plan_build()==0;assert (plan.firstRow,plan.endRow,plan.reg18)==expected;assert map_prepare()==0
 geometry.append({'screen_cell_rows':rows,'firstRow':plan.firstRow,'endRow':plan.endRow,'reg18':plan.reg18})
# Full tilemap wrapping and source-column layout parity, without rounding.
ram[:]=original
for sx,sy in ((0xffc0,0),(0x1f0,0x1f0),(0,0x100),(8,8)):
 a[6]=sx;a[7]=sy;source_epoch.value+=1;assert plan_build()==0
 expected_column=((96+sx)&511)//8;expected_row=((16+sy)&511)//8
 assert (plan.column,plan.row)==(expected_column,expected_row)
for f in ('vram.bin','cram.bin','vsram.bin','registers.bin'):
 manifest=json.loads((FIX/'manifest.json').read_text());assert hashlib.sha256((FIX/f).read_bytes()).hexdigest()==manifest['buffers'][f]['sha256']
report={'target_structure_bytes':{'WillowWindowPlan':30,'WillowWindowPriorityProof':20},'target_text_bytes':2072,'target_data_bss_bytes':0,'fixture':1200,'source_layer_control':control,'source_video_control':video,'source_draw_order_back_to_front':[3,2,0,1],'priority_masks':masks,'source_scroll2_pen_usage_under_objects':usage,'source_object_opaque_pixel_samples':samples,'unique_object_pixel_locations':len(object_points),'priority_conflicting_pens':[m&u for m,u in zip(masks,usage)],'captured_source_palette_words_verified':len(owned)*16,'captured_window_map_words_verified':1120,'captured_opaque_cells':320,'captured_window_rows':[0,10],'captured_asset_calls_inspect_resolve':captured_asset_calls,'geometry_cases':geometry,'refusal_cases':'All23 unsupported layer permutations; disabled layers; real priority-pen overlap; row scroll,global flip,unknown video mode; unaligned source scroll/crop; absent source reads/assets; stale source/palette epochs including callback mutation; high-priority asset contamination; staging failure. No partial plan may publish.','target':'Native header/callback contract; no GUI, timing, dynamic BG compositor, full-game catalog or production RAM fit claim.','files':['src/willow_native_window_plan.h','tools/test_native_window_plan.c','tools/verify_native_window_plan.py']}
(OUT/'window-plan-evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
