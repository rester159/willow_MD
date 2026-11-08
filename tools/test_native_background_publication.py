#!/usr/bin/env python3
"""Opt-in live BG bus ownership and same-IRQ snapshot extension.

Copies the strict sprite observer into this run and adds explicit BG hooks;
the shared observer is never edited. Builds without live target symbols are
permitted only with --build-only. Successful execution is not a pixel oracle.
"""
from pathlib import Path
import argparse,csv,hashlib,importlib.util,json,re,struct,subprocess
import test_native_render_cadence as cadence
from test_native_background_context import REGIONS

ROOT=Path(__file__).resolve().parents[1]
CORE_SHA='88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09'
sha=cadence.sha

def pin(path,pins,digest=None):
 path=path.resolve();actual=sha(path)
 assert digest is None or actual==digest,('Changed dependency',str(path))
 pins[str(path)]=actual;return path.read_bytes()

def sprite_builder(platform,out,pins,target):
 """Keep the source-bank spill exception tied to inspected native opcodes.

 BG preparation enlarges this one compiler local from A6-84 to A6-114.
 Neither variant relaxes dynamic EA, original-value or stack ownership proof.
 """
 image=(platform/'platform-original.bin').read_bytes();origin=0x100010
 start,end=(target['symbols'][key] for key in ['__render_code_begin','__render_code_end'])
 offsets=[]
 for source in [0x900000,0x908000]:
  prefix=bytes.fromhex('2d7c')+source.to_bytes(4,'big')
  sites=[at for at in range(start,end-7,2) if image[at-origin:at-origin+6]==prefix]
  assert len(sites)==1,('Ambiguous raw source bank immediate',sites)
  offsets.append(int.from_bytes(image[sites[0]-origin+6:sites[0]-origin+8],'big',signed=True))
 assert offsets[0]==offsets[1] and offsets[0] in (-84,-114),('Unreviewed source bank spill variant',offsets)
 background_publisher='nativeBackgroundPublish' in target['symbols']
 if offsets[0]==-84 and not background_publisher:return cadence
 original=ROOT/'tools/test_native_render_cadence.py';text=pin(original,pins).decode()
 text=cadence.replace(text,"ROOT=Path(__file__).resolve().parents[1]",'ROOT=Path('+repr(str(ROOT))+')')
 if offsets[0]==-114:
  text=cadence.replace(text,"+bytes.fromhex('ffac')","+bytes.fromhex('ff8e')")
  text=cadence.replace(text,"(choices[0],8,1,16,-84,0,0,pattern)","(choices[0],8,1,16,-114,0,0,pattern)")
  text=cadence.replace(text,"bytes.fromhex('2d6effacffde')","bytes.fromhex('2d6eff8effde')")
  text=cadence.replace(text,"(choices[0],6,2,17,-34,-84,0,pattern)","(choices[0],6,2,17,-34,-114,0,pattern)")
 if background_publisher:
  text=cadence.replace(text,"['willowPreparedObjectBind.constprop.0','nativeRenderPublish']","['willowPreparedObjectBind.constprop.0','nativeRenderPublish','nativeBackgroundPublish']")
  text=cadence.replace(text,"function=='nativeRenderPublish'","function in ('nativeRenderPublish','nativeBackgroundPublish')",count=3)
 # Derive only contiguous compiler prologues of declared BG functions.
 # Each admitted single-register push still proves dynamic SP/register value;
 # MOVEM uses the existing exact-mask proof. Helpers may execute on either
 # independently bounded native stack, so publishing=2 denotes those two ranges.
 prologues="""            for function,entry in sorted(symbols.items()):
                if not (function.startswith('bg') or function.startswith('nativeBackground')) or not start<=entry<end:continue
                at=entry
                while at<min(entry+24,end):
                    opcode=int.from_bytes(image[at-origin:at-origin+2],'big')
                    if opcode in (0x4e56,0x4fef,0x48e7):at+=4;continue
                    if 0x2f00<=opcode<=0x2f0e:
                        if not any(site[0]==at for site in sites):sites.append((at,2,0,opcode&15,0,0,2,image[at-origin:at-origin+2]))
                        at+=2;continue
                    break
"""
 text=cadence.replace(text,"            candidate=products[3].read_bytes()",prologues+"            candidate=products[3].read_bytes()")
 local=out/'sprite-builder-background-provenance.py';local.write_text(text);pin(local,pins)
 spec=importlib.util.spec_from_file_location('background_sprite_builder',local);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
 return module

def build(platform,assets,out,build_only=False,collect_register_failures=False):
 platform,assets,out=platform.resolve(),assets.resolve(),out.resolve()
 assert not out.exists(),'Use a fresh evidence directory'
 out.mkdir(parents=True);pins={}
 target=json.loads(pin(platform/'manifest.json',pins))
 definitions=pin(platform/'render-addresses.h',pins).decode()
 live=bool(re.search(r'^#define RENDER_BACKGROUND_LIVE 1$',definitions,re.M))
 assert live or build_only,'Target has no explicit live BG observer contract'
 values={name:int(value.rstrip('uUlL'),0) for name,value in re.findall(r'^#define (RENDER_BG_\w+) (0x[0-9a-fA-F]+[uUlL]*|[0-9]+[uUlL]*)$',definitions,re.M)}
 for name,digest in target['products'].items():pin(platform/name,pins,digest)
 if live:
  for name in ['RENDER_BG_CODE_BEGIN','RENDER_BG_CODE_END','RENDER_BG_RTE_PC','RENDER_BG_IRQ_SP']:assert name in values,name
  assert values['RENDER_BG_CODE_BEGIN']>=0x100000 and values['RENDER_BG_CODE_END']<=0x110000
  assert values['RENDER_BG_CODE_BEGIN']<=values['RENDER_BG_RTE_PC']<values['RENDER_BG_CODE_END']
  assert 0xff4000<=values['RENDER_BG_IRQ_SP']<=0xff6ffc
  for name in ['platform-original.bin','platform.bin']:
   image=(platform/name).read_bytes();offset=values['RENDER_BG_RTE_PC']-0x100010
   assert image[offset-4:offset+2]==bytes.fromhex('4cdf7fff4e73'),'Native BG return must preserve original MOVEM/RTE bytes'
  irq_destinations=[]
  for name in ['program-original.bin','program.bin']:
   image=(platform/name).read_bytes();assert image[0x15b4:0x15b6]==bytes.fromhex('4ef9')
   destination=int.from_bytes(image[0x15b6:0x15ba],'big')
   assert values['RENDER_BG_CODE_BEGIN']<=destination<values['RENDER_BG_CODE_END']
   irq_destinations.append(destination)
  assert irq_destinations[0]==irq_destinations[1]
 for name,digest in target['resource_files'].items():pin(Path(name),pins,digest)
 for name,digest in target['source_files'].items():
  original=Path(name) if Path(name).is_absolute() else ROOT/name
  data=original.read_bytes();assert hashlib.sha256(data).hexdigest()==digest,('Target source changed before snapshot',name)
  copy=out/'frozen-target-sources'/original.relative_to(ROOT);copy.parent.mkdir(parents=True,exist_ok=True);copy.write_bytes(data);pin(copy,pins,digest)
 prepared=json.loads(pin(assets/'manifest.json',pins))
 assert target['resource_files'].get(str(assets/'manifest.json'))==pins[str(assets/'manifest.json')],'Target does not pin selected BG manifest'
 for name,digest in prepared['outputs'].items():pin(assets/name,pins,digest)
 for name,digest in prepared['hashes'].items():pin(Path(name) if Path(name).is_absolute() else ROOT/name,pins,digest)
 assert prepared['reserved_tiles']=={'low':[0,15],'window':[876,980],'objects':[981,1480],'tables_begin':1536}
 patterns=bytearray(1536*32);tiles=set()
 for tile,data in struct.iter_unpack('>H32s',(assets/'physical-tiles.bin').read_bytes()):
  assert tile not in tiles and (16<=tile<=875 or 1481<=tile<=1535)
  tiles.add(tile);patterns[tile*32:tile*32+32]=data
 assert len(tiles)==prepared['uploaded_patterns']==906
 objects=[Path(name).parent for name in target['resource_files'] if name.endswith('/route_objects.h')];assert len(objects)==1
 object_data=(objects[0]/'object-patterns.bin').read_bytes();assert len(object_data)==16000
 assert (objects[0]/'cram.bin').read_bytes()==(assets/'cram.bin').read_bytes()
 patterns[981*32:1481*32]=object_data
 expected=out/'background-publication-assets.h'
 expected.write_text('/* Host-only immutable pattern oracle. */\nstatic const uint8_t backgroundExpectedPatterns[]={\n'+'\n'.join(','.join(map(str,patterns[n:n+32]))+',' for n in range(0,len(patterns),32))+'\n};\n')
 pin(expected,pins)
 binary,manifest=sprite_builder(platform,out,pins,target).build(platform,out,extra_rom=True)
 own=out/'frozen-observer-tools';own.mkdir()
 for original in [Path(__file__),ROOT/'tools/test_native_background_publication.h']:
  copy=own/original.name;copy.write_bytes(original.read_bytes());pin(copy,pins)
 header=own/'test_native_background_publication.h'
 shared=ROOT/'tools/test_native_render_cadence_observer.h'
 observer=shared.read_text()
 declarations='''static unsigned backgroundCodePC(unsigned);
static unsigned backgroundRTEPC(unsigned);
static unsigned backgroundInterruptedMovem(unsigned,unsigned);
static void backgroundPhase(unsigned);
static void backgroundPhaseByte(void);
static unsigned backgroundVDPBefore(unsigned,unsigned,unsigned);
static void backgroundVDPAfter(unsigned,unsigned);
static void backgroundBeforeWork(unsigned,unsigned,unsigned);
static void backgroundReadWork(unsigned);
'''
 observer=declarations+observer
 observer=cadence.replace(observer,'base<(site->publishing?0x7600:0x7000)','base<(site->publishing==1?0x7600:0x7000)')
 observer=cadence.replace(observer,'ownerPC!=0x1750||pc!=0x1754||','((ownerPC!=0x1750||pc!=0x1754)&&!backgroundInterruptedMovem(ownerPC,pc))||')
 observer=cadence.replace(observer,'#include "test_native_render_cadence_oracle.h"','#include "'+str(ROOT/'tools/test_native_render_cadence_oracle.h')+'"')
 observer=cadence.replace(observer,' if(pc<0x100000)return 1;',' if(pc<0x100000||backgroundCodePC(pc))return 1;')
 observer=cadence.replace(observer,'static void renderBeforeWork(unsigned at,unsigned width,unsigned value){','static void renderBeforeWork(unsigned at,unsigned width,unsigned value){\n backgroundBeforeWork(at,width,value);')
 observer=cadence.replace(observer,'static unsigned renderNativeRead8(unsigned at){','static unsigned renderNativeRead8(unsigned at){\n backgroundReadWork(at);')
 observer=cadence.replace(observer,'(sourcePC()==0x15b8||sourcePC()==0x15ba)','backgroundRTEPC(sourcePC())')
 observer=cadence.replace(observer,'static void renderSRAM8(unsigned at,unsigned value){renderBusSRAM8(at,value);','static void renderSRAM8(unsigned at,unsigned value){renderBusSRAM8(at,value);if(at==0x20f590||at==0x20f591)backgroundPhaseByte();')
 observer=cadence.replace(observer,'static void renderSRAM16(unsigned at,unsigned value){renderBusSRAM16(at,value);','static void renderSRAM16(unsigned at,unsigned value){renderBusSRAM16(at,value);if(at==0x20f590)backgroundPhase(value&65535);')
 for width in [8,16]:
  before=f'static void renderVDP{width}(unsigned at,unsigned value){{renderBusVDP{width}(at,value);renderVDPAfter(at);}}'
  after=f'static void renderVDP{width}(unsigned at,unsigned value){{unsigned bg=backgroundVDPBefore(at,{width//8},value);renderBusVDP{width}(at,value);if(bg)backgroundVDPAfter(at,value);else renderVDPAfter(at);}}'
  observer=cadence.replace(observer,before,after)
 private=out/'sprite-observer-with-background-hooks.h';private.write_text(observer);pin(private,pins)
 generated=out/'test_native_render_cadence.c';text=generated.read_text()
 if live:
  text=cadence.replace(text,'willowMotionWord(p+0x15b8)!=0x4e73',f'(willowMotionWord(p+0x15b4)!=0x4ef9||willowMotionLong(p+0x15b6)!=0x{irq_destinations[0]:x}u)')
 text=cadence.replace(text,'#include "'+str(shared)+'"','#include "'+str(private)+'"\n#include "'+str(expected)+'"\n#include "'+str(header)+'"')
 text=cadence.replace(text,' if(!renderInit(core))return 2;',' if(!renderInit(core))return 2;\n if(!backgroundInit(core))return 2;')
 text=cadence.replace(text,'   if(renderObserveFrame(path))return 1;','   if(renderObserveFrame(path))return 1;\n   if(backgroundObserveFrame(path))return 1;')
 text=cadence.replace(text,' renderFinish();',' backgroundFinish();\n renderFinish();')
 if collect_register_failures:
  text=cadence.replace(text,'int main(', 'static unsigned backgroundRegisterFailures;\nstatic FILE*backgroundRegisterLog;\nint main(')
  old='fprintf(stderr,"Cadence register mismatch frame%u r%u original%x native%x\\n",frameNumber,n,originalRegs[n],m68k_get_reg(n));return 1;'
  new='fprintf(stderr,"DIAGNOSTIC NOT ADMITTED: register mismatch frame%u r%u original%x native%x originalPC%x nativePC%x\\n",frameNumber,n,originalRegs[n],m68k_get_reg(n),originalRegs[16],m68k_get_reg(16));\n     if(!backgroundRegisterLog){backgroundRegisterLog=outputFile("register-failures.csv","w");if(!backgroundRegisterLog)abort();fputs("frame,reg,original,native,original_pc,native_pc,original_sr,native_sr,original_sp,native_sp\\n",backgroundRegisterLog);}\n     fprintf(backgroundRegisterLog,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n",frameNumber,n,originalRegs[n],m68k_get_reg(n),originalRegs[16],m68k_get_reg(16),originalRegs[17],m68k_get_reg(17),originalRegs[15],m68k_get_reg(15));fflush(backgroundRegisterLog);backgroundRegisterFailures++;'
  text=cadence.replace(text,old,new)
  text=cadence.replace(text,'full sourceRAM/sevenvideo/ports/registers/nativeIRQobservations and residualcycles match.','full sourceRAM/sevenvideo/ports/nativeIRQobservations and residualcycles match; register mismatches are retained separately and forbid admission.')
  text=cadence.replace(text,' free(states[0]);free(states[1]);return 0;', ' if(backgroundRegisterLog)fclose(backgroundRegisterLog);\n fprintf(stderr,"Diagnostic register failures: %u; %s\\n",backgroundRegisterFailures,backgroundRegisterFailures?"NOT ADMITTED":"no register failure observed");\n free(states[0]);free(states[1]);return backgroundRegisterFailures?1:0;')
 generated.write_text(text);subprocess.run(manifest['command'],cwd=ROOT,check=True)
 for path in [header,ROOT/'tools/test_native_background_context.py',ROOT/'build/production-rom-audit/genplus-vdp_ctrl-5a8389c.c']:pin(path,pins)
 manifest['inputs'].update(pins)
 manifest.update(generated_sha256=sha(generated),binary_sha256=sha(binary),extension_scope=__doc__,live_bg_contract=live,diagnostic_register_failure_collection=collect_register_failures,background_assets=str(assets),background_core_abi_sha256=CORE_SHA)
 (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 return binary,manifest

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--platform',type=Path,default=ROOT/'build/native-render-cadence/background-live-v1')
 parser.add_argument('--assets',type=Path,default=ROOT/'build/native-background-residency/prepared-v1')
 parser.add_argument('--output',type=Path,default=ROOT/'build/native-background-publication/observer-v1')
 parser.add_argument('--frames',type=int,default=1800)
 parser.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
 parser.add_argument('--build-only',action='store_true')
 parser.add_argument('--collect-register-failures',action='store_true',help='Diagnostic only: retain all register mismatches, keep all other stops, exit1 if any mismatch; never admits differing state')
 args=parser.parse_args();assert 1<=args.frames<=36000
 out=args.output.resolve();binary,manifest=build(args.platform,args.assets,out,args.build_only,args.collect_register_failures)
 if args.build_only:print(binary);return
 assert sha(args.core)==CORE_SHA,'VDP private context layout requires the pinned headless core'
 with(out/'observer.log').open('w')as log:result=subprocess.run([str(binary),str(args.core),str(args.frames),'--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 manifest['run']={'exit_code':result.returncode,'frames':args.frames,'core_sha256':sha(args.core),'log_sha256':sha(out/'observer.log')}
 (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 assert all(sha(Path(name))==digest for name,digest in manifest['inputs'].items()),'Observer dependency changed during run'
 if result.returncode:raise SystemExit(result.returncode)
 summary=json.loads((out/'background-summary.json').read_text());samples=list(csv.DictReader((out/'background-publications.csv').open()))
 assert len(samples)==summary['samples'] and len(samples)>0,'No successful BG publications; no admission'
 offset=0;regions=[]
 for name,size,address in REGIONS:regions.append({'name':name,'offset':offset,'size':size,'source_start':address});offset+=size
 assert (out/'background-source-contexts.bin').stat().st_size==offset*len(samples)
 assert (out/'background-vram.bin').stat().st_size==65536*len(samples)
 assert (out/'background-vdp-state.bin').stat().st_size==288*len(samples)
 layout={'schema_version':1,'boundary':'Successful reference BG phase4, after original source IRQ effects and explicit native BG publication; descriptor checked against current800110..116. Cold partial snapshots are explicitly flagged and are not complete display admission.','samples':len(samples),'index':'background-publications.csv','source':{'file':'background-source-contexts.bin','record_bytes':offset,'regions':regions},'vram':{'file':'background-vram.bin','record_bytes':65536,'byte_order':'canonical big endian'},'vdp_state':{'file':'background-vdp-state.bin','record_bytes':288,'regions':[{'name':'cram_bus','offset':0,'size':128},{'name':'vsram','offset':128,'size':128},{'name':'registers','offset':256,'size':32}]},'final_source':'background-final-source.bin','final_vdp':'Existing render-final-vram.bin/cram.bin/registers.bin files remain unchanged sprite-verifier outputs.','deadline':'Conservative same first-SAT-fetch boundary126540 master ticks after frame start, including observed FIFO retirement.','pixel_oracle_admitted':False}
 (out/'background-layout.json').write_text(json.dumps(layout,indent=2)+'\n')
 print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
