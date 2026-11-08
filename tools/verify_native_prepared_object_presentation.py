#!/usr/bin/env python3
"""Prepare source-derived templates, then reuse the full-bank native VDP gate.

This freezes frame1200's template/instance topology as an integration fixture.
Live source emission must maintain that topology at the original latch boundary.
"""
from pathlib import Path
import argparse,hashlib,json,re,runpy,sys
import build_vdp_presentation_probe as probe
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--certified-empty',action='store_true');p.add_argument('--native-cells',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'build/native-prepared-objects/frame1200');p.add_argument('--fixture',type=Path,default=ROOT/'build/cartridge-gap-audit/frame1200_window_v1');p.add_argument('--asset-binding',type=Path,default=ROOT/'build/cartridge-gap-audit/frame1200_window_v1/asset-binding.json');args=p.parse_args()
original_build=probe.build
def selected_build(inputs,out,**kwargs):
 assert kwargs.get('native_source')==ROOT/'tools/native_latch_presentation.c'
 header=(Path(kwargs.get('native_include') or out)/'latch_fixture.h').read_text()
 match=re.search(r'fixtureAssets\[\]=\{(.*?)\};',header,re.S);assert match
 triples=[tuple(map(int,m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\}',match.group(1))]
 assets={key:(attribute,status) for key,attribute,status in triples};assert len(assets)==len(triples)
 match=re.search(r'fixtureBank\[\].*?=\{(.*?)\};',header,re.S);assert match
 bank=[int(n,0) for n in match.group(1).split(',') if n.strip()];assert len(bank)==1024
 left,top,width,height,global_flip=96,16,320,224,0
 instances=[];templates=[];source_records=0;omitted=[]
 for ordinal in range(256):
  x,y,base,attribute=bank[ordinal*4:ordinal*4+4]
  if attribute&0xff00==0xff00:break
  source_records+=1
  if base>0x27ff:omitted.append({'ordinal':ordinal,'reason':'source code outside original object mapper'});continue
  columns=((attribute>>8)&15)+1;rows=(attribute>>12)+1
  visible=False
  for row in range(rows):
   for column in range(columns):
    sx=((x+column*16)&511)-left;sy=((y+row*16)&511)-top
    if -16<sx<320 and -16<sy<224:visible=True
  if not visible:omitted.append({'ordinal':ordinal,'reason':'complete original block outside bound source view'});continue
  cells=[]
  for row in range(rows):
   for column in range(columns):
    cx=columns-1-column if attribute&32 else column;cy=rows-1-row if attribute&64 else row
    code=(base&0xfff0)+((base+cx)&15)+cy*16;key=(code<<5)|(attribute&31)
    if key not in assets:raise ValueError(f'Entire template requires source code/group asset {code:04x}/{attribute&31}, including currently offscreen children')
    native,status=assets[key]
    if status==2:continue
    if status!=1 or native&0x1800 or native&0x7ff>2044:raise ValueError('Invalid explicit resident descriptor')
    cells.append({'offset':(row*16<<8)|(column*16),'attribute':native|((attribute<<6)&0x1800),'source_code':code})
  template={'base':base,'attribute':attribute,'cells':cells}
  if template not in templates:templates.append(template)
  instances.append({'ordinal':ordinal,'template':templates.index(template),'source_x':x,'source_y':y})
 assert source_records==256 and len(instances)==3,'This first prepared gate requires the complete original1200 bank'
 generated='#include "willow_native_object_prepared.h"\n'
 for n,t in enumerate(templates):
  generated+=f'static const WillowPreparedObjectCell preparedCells{n}[]={{'+(','.join('{'+str(c['offset'])+','+str(c['attribute'])+'}' for c in t['cells']) or '{0,0}')+'};\n'
  generated+=f'static const WillowPreparedObjectTemplate preparedTemplate{n}={{preparedCells{n},{len(t["cells"])},{t["base"]},{t["attribute"]}}};\n'
 generated+=f'static WillowPreparedObjectInstance preparedInstances[{len(instances)}];\nstatic WillowPreparedObjectFrame preparedFrame;\nstatic WillowPreparedObjectBinding preparedBinding;\nstatic const WillowPreparedObjectEpoch preparedExpected={{1,77,77,1}};\n'
 generated+='static uint8_t preparedFixtureInit(void){\n'
 for n,i in enumerate(instances):
  generated+=f'preparedInstances[{n}].shape=&preparedTemplate{i["template"]};preparedInstances[{n}].sourceX=0;preparedInstances[{n}].sourceY=0;preparedInstances[{n}].sourceOrdinal={i["ordinal"]};\n'
 generated+=f'preparedFrame.instances=preparedInstances;preparedFrame.count={len(instances)};preparedFrame.sourceRecords=256;\n'
 generated+='preparedFrame.epoch.latch=0;preparedFrame.epoch.resident=77;preparedFrame.epoch.palette=77;preparedFrame.epoch.view=1;\npreparedFrame.view.left=96;preparedFrame.view.top=16;preparedFrame.view.width=320;preparedFrame.view.height=224;preparedFrame.view.flipScreen=0;\nreturn willowPreparedObjectBind(&preparedBinding,&preparedFrame)==0;\n}\n'
 generated+='static uint8_t preparedFixtureLatch(const volatile uint8_t*ram){\nif(!willowPreparedObjectLatchBegin(&preparedBinding))return 0;\nuint16_t words[4];\n'
 for i in instances:
  generated+=f'if(!willowNativeObjectLatchReadRecord(ram,{i["ordinal"]},words)||!willowPreparedObjectLatchOrigin(&preparedBinding,{i["ordinal"]},words[0],words[1],words[2],words[3]))return 0;\n'
 generated+='return willowPreparedObjectLatchSeal(&preparedBinding,1);}\n'
 (out/'prepared_fixture.h').write_text(generated)
 (out/'prepared-plan.json').write_text(json.dumps({'scope':'Static1200 topology; origins are refreshed natively from actual raw latch','templates':templates,'instances':instances,'source_records':source_records,'omitted':omitted,'view':[left,top,width,height,global_flip],'cell_bytes':sum(len(t['cells'])*4 for t in templates),'template_bytes':len(templates)*10,'instance_bytes':len(instances)*10,'epoch':{'latch':1,'resident':77,'palette':77,'view':1}},indent=2)+'\n')
 source=(ROOT/'tools/native_latch_presentation.c').read_text()
 source=source.replace('#include "latch_fixture.h"','#include "latch_fixture.h"\n#include "prepared_fixture.h"')
 source=source.replace('static WillowObjectSatPiece pieces[80];','')
 start=source.index('static uint8_t residentAsset(');end=source.index('\nuint16_t nativeObjectPresentation',start);source=source[:start]+source[end:]
 source=source.replace(' *(volatile uint16_t*)0xff0004=1;',' if(!preparedFixtureInit())return 0;\n *(volatile uint16_t*)0xff0004=1;')
 source=source.replace('willowObjectSatBudgetInit(&budget)','willowNativeObjectSatBudgetInitM68k(&budget)').replace('willowObjectSatReserve(&budget','willowNativeObjectSatReserveM68k(&budget')
 start=source.index(' const WillowObjectSatView view=');end=source.index(' if(result.status',start)
 source=source[:start]+''' if(!preparedFixtureLatch(ram))return 0;
 WillowObjectSatResult result=willowPreparedObjectBuild(&preparedBinding,&preparedExpected,&budget,staged,80,lineDeltas);
'''+source[end:]
 start=source.index(' for(unsigned n=0;n<result.pieces;n++){');end=source.index(' *(volatile uint16_t*)0xff0004=4;',start);source=source[:start]+source[end:]
 if args.certified_empty:
  assert '#define FIXTURE_TEXT_COUNT 0' in header
  source=source.replace('#include "prepared_fixture.h"','#include "prepared_fixture.h"\nstatic WillowPreparedObjectSeed preparedSeed;')
  source=source.replace('if(!preparedFixtureInit())return 0;','if(!preparedFixtureInit()||!willowPreparedObjectSeedEmpty(&preparedSeed,1))return 0;')
  source=source.replace(' willowNativeObjectSatBudgetInitM68k(&budget);',' /* Certified empty constructor initializes owned output inside BuildSeeded. */')
  source=source.replace('willowPreparedObjectBuild(&preparedBinding,&preparedExpected,&budget,staged,80,lineDeltas)','willowPreparedObjectBuildSeeded(&preparedBinding,&preparedExpected,&preparedSeed,1,&budget,staged,80,lineDeltas)')
 if args.native_cells:source='#define WILLOW_PREPARED_NATIVE_CELLS 1\n'+source
 (out/'prepared_native.c').write_text(source)
 kwargs['native_source']=out/'prepared_native.c';kwargs['native_include']=out
 return original_build(inputs,out,**kwargs)
probe.build=selected_build
sys.argv=[str(ROOT/'tools/verify_native_latch_presentation.py'),'--deferred-budget','--fixture',str(args.fixture),'--asset-binding',str(args.asset_binding),'--output',str(args.output)]
runpy.run_path(str(ROOT/'tools/verify_native_latch_presentation.py'),run_name='__main__')
path=args.output.resolve()/'evidence.json';e=json.loads(path.read_text());e['scope']='Prepared native object templates/static1200 topology; native latch origins,direct native SAT and actual VDP; no live source producer integration or FPS admission';e['preparation_excluded_from_publication_timing']=True;e['native_origin_refresh_from_actual_latch']=True;e['certified_empty_seed']=args.certified_empty;e['native_cell_loop']=args.native_cells;e['asset_callback']='None during timed publication; source-code/group assets were resolved into immutable prepared template attributes before binding'
for file in [Path(__file__),ROOT/'src/willow_native_object_prepared.h',ROOT/'src/willow_native_object_cells_m68k.h',ROOT/'src/willow_native_object_sat_budget_m68k.h',args.output/'prepared_fixture.h',args.output/'prepared-plan.json',args.output/'prepared_native.c']:e['hashes'][str(file.resolve())]=hashlib.sha256(file.read_bytes()).hexdigest()
path.write_text(json.dumps(e,indent=2)+'\n');print(json.dumps(e['timing'],indent=2))
