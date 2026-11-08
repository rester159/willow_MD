#!/usr/bin/env python3
"""Extend ordinary source-controlled right/attack play beyond the opening.

No service controls, invulnerability, state patches, captured positions or
target-code changes. All source/VDP checks remain, with explicit register
diagnostics. Stop after the first proven unsupported resource and paired frame.
"""
from pathlib import Path
import argparse
import csv
import json
import re
import struct
import subprocess
import types
import test_native_background_publication as background
from test_native_background_context import REGIONS

ROOT=Path(__file__).resolve().parents[1]


def main():
    global background
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform',type=Path,default=ROOT/'build/native-background-publication/target-live-v8-frozen')
    p.add_argument('--assets',type=Path,default=ROOT/'build/native-background-residency/prepared-v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-forest-extension/v8')
    p.add_argument('--active-frames',type=int,default=1800)
    p.add_argument('--frames',type=int,default=3600)
    p.add_argument('--observe-main-body',action='store_true',help='Read-only opcode-guarded original93BE body completion and sleep observations')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--frozen-source',action='append',default=[],metavar='ORIGINAL=FROZEN',help='Resolve an old declared source dependency only to an exact matching frozen SHA; assets/products remain strict')
    a=p.parse_args();assert 180<a.active_frames<=18000 and a.active_frames<a.frames<=24000
    out=a.output.resolve();platform=a.platform.resolve()
    # The baseline fixture intentionally pins its original 906-pattern layout.
    # This separate route wrapper admits only the same 915 physically available
    # slots, still validating every declared pattern and every other VRAM byte.
    builder_path=ROOT/'tools/test_native_background_publication.py'
    builder_text=background.cadence.replace(builder_path.read_text(),
        " assert len(tiles)==prepared['uploaded_patterns']==906",
        " assert 0<len(tiles)==prepared['uploaded_patterns']<=915")
    module=types.ModuleType('forest_background_builder');module.__file__=str(builder_path)
    exec(compile(builder_text,str(builder_path),'exec'),module.__dict__)
    background=module
    aliases={}
    for entry in a.frozen_source:
        original,frozen=entry.split('=',1);original,frozen=Path(original).resolve(),Path(frozen).resolve()
        assert original not in aliases and original.suffix in ('.py','.h','.c','.S') and frozen.is_file()
        aliases[original]=frozen
    original_pin=background.pin
    def frozen_pin(path,pins,digest=None):
        path=path.resolve()
        if digest is not None and path in aliases:
            return original_pin(aliases[path],pins,digest)
        return original_pin(path,pins,digest)
    background.pin=frozen_pin
    binary,manifest=background.build(platform,a.assets,out,collect_register_failures=True)
    private_builder=out/'forest-background-builder.py';private_builder.write_text(builder_text)
    target=json.loads((platform/'manifest.json').read_text());keys_path=next(Path(n) for n in target['resource_files'] if n.endswith('/route_objects.h'))
    # Bind source-view acquisition to exact generated instructions. This one
    # supported form consists of four absolute word loads, with native locals
    # preserving the complete tuple across the original IRQ.
    code_origin=0x100010;image=(platform/'platform-original.bin').read_bytes()
    entry=target['symbols']['nativeBackgroundPrepare'];end=min(v for v in target['symbols'].values() if v>entry)
    masked=re.escape(bytes.fromhex('40c0007c0700'))+b''.join(bytes.fromhex('3d79')+struct.pack('>I',0xfff446+n*2)+b'..' for n in range(4))+bytes.fromhex('46c0')
    expression=bytes.fromhex('303900fff4463d40')+b'..'+bytes.fromhex('3d7900fff448')+b'..'+bytes.fromhex('3d7900fff44a')+b'..'+bytes.fromhex('3d7900fff44c')+b'..'
    movem=bytes.fromhex('4cb9000f00fff4463280334100023342000433430006')
    found=list(re.finditer(re.escape(movem),image[entry-code_origin:end-code_origin],re.S))
    atomic=bool(found);protected=False
    if not found:
        found=list(re.finditer(masked,image[entry-code_origin:end-code_origin],re.S));protected=bool(found)
    if not found:found=list(re.finditer(expression,image[entry-code_origin:end-code_origin],re.S))
    assert len(found)==1,'Unknown generated source-view acquisition'
    begin=entry+found[0].start();acquisition=found[0].group();length=22 if atomic else 40 if protected else 34
    offsets=(8,8,8,8) if atomic else (12,20,28,36) if protected else (6,16,24,32)
    assert len(acquisition)==length and not begin&1
    if not atomic:
        displacements=[struct.unpack_from('>h',acquisition,n)[0] for n in ((12,20,28,36) if protected else (8,16,24,32))]
        assert displacements==list(range(displacements[0],displacements[0]+8,2))
    marker=bytes.fromhex('33fc00010020f590');sites=[at for at in range(begin+length,end-7,2) if image[at-code_origin:at-code_origin+8]==marker];assert len(sites)==1
    candidate=(platform/'platform.bin').read_bytes();assert candidate[begin-code_origin:begin-code_origin+length]==acquisition and candidate[sites[0]-code_origin:sites[0]-code_origin+8]==marker
    view_sites=out/'forest-view-acquisition-sites.h'
    view_sites.write_text('/* Exact generated native source-view acquisition; host only. */\n'+('#define FOREST_VIEW_MOVEM 1\n' if atomic else '')+
        f'#define FOREST_VIEW_LOAD_BEGIN 0x{begin:x}u\n#define FOREST_VIEW_ACQUISITION_END 0x{begin+length:x}u\n#define FOREST_VIEW_MARKER_NEXT 0x{sites[0]+8:x}u\n'+
        # MOVE absolute->d16(A6) reads its source before fetching the destination
        # displacement word; its bus callback is opcode+6, not instruction+8.
        'static const unsigned forestViewLoadNextPC[]={'+','.join(hex(begin+n)+'u' for n in offsets)+'};\n'+
        'static const uint8_t forestViewOriginalCode[]={'+','.join(map(str,acquisition))+'};\n'+
        'static const uint8_t forestViewNativeCode[]={'+','.join(map(str,acquisition))+'};\n')
    matches=re.search(r'routeKeys\[\]=\{(.+?)\};',keys_path.read_text()).group(1)
    object_keys=[int(x) for x in re.findall(r'\{(\d+)UL\}',matches)];assert object_keys==sorted(set(object_keys)) and len(object_keys)==17
    imports=platform/'render_background_imports.h';defines={n:int(v.rstrip('UL'),0) for n,v in re.findall(r'^#define (WILLOW_BG_\w+) (0x[0-9a-fA-F]+UL|\d+U?L?)$',imports.read_text(),re.M)}
    blob=(platform/target['extra_rom']['file']).read_bytes();origin=target['extra_rom']['origin']
    tables={'forestObjectKeys':object_keys}
    for layer in [2,3]:
        at=defines[f'WILLOW_BG_CATALOG_LAYER{layer}_PAIRS_ADDRESS']-origin;count=defines[f'WILLOW_BG_CATALOG_LAYER{layer}_PAIR_COUNT']
        assert 0<=at<at+count*6<=len(blob)
        keys=[struct.unpack_from('>I',blob,at+n*6)[0] for n in range(count)];assert keys==sorted(set(keys));tables[f'forestBGKeys{layer}']=keys
        cells=4 if layer==2 else 16
        offsets=[struct.unpack_from('>H',blob,at+n*6+4)[0] for n in range(count)]
        assert offsets==list(range(0,count*cells,cells))
        refs=defines[f'WILLOW_BG_CATALOG_LAYER{layer}_REFS_ADDRESS']-origin
        opacity=defines[f'WILLOW_BG_CATALOG_LAYER{layer}_OPACITY_ADDRESS']-origin
        assert 0<=refs<refs+count*cells*4<=len(blob) and 0<=opacity<opacity+count*cells*8<=len(blob)
        tables[f'forestBGWords{layer}']=[struct.unpack_from('>H',blob,refs+n*4+2)[0] for n in range(count*cells)]
        tables[f'forestBGPatterns{layer}']=[struct.unpack_from('>H',blob,refs+n*4)[0] for n in range(count*cells)]
        tables[f'forestBGOpacity{layer}']=list(blob[opacity:opacity+count*cells*8])
    host=out/'forest-resource-keys.h';host.write_text('/* Host-only keys copied byte-exact from the declared immutable target assets. */\n'+''.join('static const uint32_t '+name+'[]={'+','.join(f'0x{k:08x}u' for k in keys)+'};\n' for name,keys in tables.items()))
    own=out/'frozen-observer-tools';header=own/'test_native_forest_extension.h';header.write_bytes((ROOT/'tools/test_native_forest_extension.h').read_bytes());(own/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    view_header=own/'test_native_forest_view_acquisition.h';view_header.write_bytes((ROOT/'tools/test_native_forest_view_acquisition.h').read_bytes())
    private=out/'sprite-observer-with-background-hooks.h';text=private.read_text();decl='static void forestReject(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);\n';text=decl+text
    text='static void forestBGRead(unsigned,unsigned,unsigned);\nstatic void forestBGBeforeWrite(unsigned,unsigned,unsigned);\n'+text
    text=background.cadence.replace(text,'static unsigned renderNativeRead16(unsigned at){if(at&1)abort();return renderNativeRead8(at)<<8|renderNativeRead8(at+1);}',
        'static unsigned renderNativeRead16(unsigned at){if(at&1)abort();unsigned value=renderNativeRead8(at)<<8|renderNativeRead8(at+1);forestBGRead(at,2,value);return value;}')
    text=background.cadence.replace(text,'static void renderBeforeWork(unsigned at,unsigned width,unsigned value){','static void renderBeforeWork(unsigned at,unsigned width,unsigned value){\n forestBGBeforeWrite(at,width,value);')
    needle='  renderEvent(publishing?"publish_reject":"prepare_reject",&end,status);'
    text=background.cadence.replace(text,needle,needle+'\n  forestReject(0,publishing,status,end.bank,end.records,end.epoch,end.palette);');private.write_text(text)
    bg=own/'test_native_background_publication.h';text=bg.read_text();needle='  p->rejects++;if(renderActive())p->activeRejects++;backgroundEvent(phase,status,publishing?&p->publish:&p->prepare);'
    text=background.cadence.replace(text,needle,needle+'\n  forestReject(1,publishing,status,renderWord(0xf544),renderWord(0xf546),renderLong(0xf548),renderLong(0xf568));')
    text=background.cadence.replace(text,' if(!backgroundViewCurrent(s,publishing))backgroundFail("descriptor does not match current source view");',
        ' if(!publishing)forestBGPrepareView(s->view,s->generation);\n else if(!backgroundViewCurrent(s,publishing))backgroundFail("descriptor does not match current source view");')
    bg.write_text(text)
    generated=out/'test_native_render_cadence.c';text=generated.read_text()
    text=background.cadence.replace(text,'#include "'+str(bg)+'"','#include "'+str(view_sites)+'"\n#include "'+str(view_header)+'"\n#include "'+str(bg)+'"')
    text=background.cadence.replace(text,'static unsigned sourcePC(void);','static unsigned sourcePC(void);\nstatic void forestBGReferenceRead(unsigned,unsigned,unsigned);')
    needle='static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){'
    text=background.cadence.replace(text,needle,needle+'\n if(kind==0)forestBGReferenceRead(at,width,value);')
    text=background.cadence.replace(text,' if(!backgroundInit(core))return 2;',' if(!backgroundInit(core))return 2;\n forestViewCheckCode();')
    text=background.cadence.replace(text,'int main(', '#include "'+str(host)+'"\n#include "'+str(header)+'"\nint main(')
    text=background.cadence.replace(text,'if(routeTicks<180)rawPlayer',f'if(routeTicks<{a.active_frames})rawPlayer')
    text=background.cadence.replace(text,'  unsigned originalRegs[21],originalCycles=0;','  forestActiveTicks=routeTicks;\n  unsigned originalRegs[21],originalCycles=0;')
    text=background.cadence.replace(text,'routeTicks);fflush(stdout);','routeTicks);fflush(stdout);\n  if(forestCoverageStop){forestStopAfterPairedFrame(routeTicks);return 3;}')
    body_source=None
    if a.observe_main_body:
        import test_native_main_loop_cadence as body
        body_source=own/'test_native_main_loop_cadence.py';body_source.write_bytes(Path(body.__file__).read_bytes())
        text=background.cadence.replace(text,'static unsigned sourcePC(void);','static unsigned sourcePC(void);\nstatic void mainLoopObserve(unsigned,unsigned,unsigned,unsigned);')
        needle='static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){'
        text=background.cadence.replace(text,needle,needle+'\n mainLoopObserve(kind,at,width,value);')
        hooks=body.HOOKS
        irq_header=own/'test_native_forest_body_irq.h';irq_header.write_bytes((ROOT/'tools/test_native_forest_body_irq.h').read_bytes())
        hooks=background.cadence.replace(hooks,'static void mainLoopObserve(', '#include "'+str(irq_header)+'"\nstatic void mainLoopObserve(')
        hooks=background.cadence.replace(hooks,' unsigned pc=sourcePC(),sp=cpu->dar[15]&0xffffff;', ' unsigned pc=sourcePC(),sp=cpu->dar[15]&0xffffff;\n if(mainBodyIRQMarker(kind,at,width,value))return;')
        hooks=background.cadence.replace(hooks,'  if(value!=pc||(rawLong(0x204)&0xffffff)!=0xff0060)abort();','  mainBodyCallStack(at,value);\n  if(value!=pc||(rawLong(0x204)&0xffffff)!=0xff0060)abort();')
        start=hooks.index('static void mainLoopObserve(');end=hooks.index('static void mainLoopCheckCode(')
        observed=hooks[start:end]
        assert observed.count('abort();')==4
        observed=observed.replace('abort();','do{fprintf(stderr,"Main-body observation invariant line%u frame%u kind%u at%x width%u value%x PC%x SP%x task%x body%u open%u\\n",__LINE__,frameNumber,kind,at,width,value,pc,sp,rawLong(0x204)&0xffffff,mainBodyNumber,mainBodyOpen);abort();}while(0);')
        hooks=hooks[:start]+observed+hooks[end:]
        text=background.cadence.replace(text,'int main(',hooks+'\nint main(')
        text=background.cadence.replace(text,' if(!backgroundInit(core))return 2;',' if(!backgroundInit(core))return 2;\n mainLoopCheckCode();')
        text=background.cadence.replace(text,' backgroundFinish();',' if(mainLoopLog)fclose(mainLoopLog);\n backgroundFinish();')
    generated.write_text(text);subprocess.run(manifest['command'],cwd=ROOT,check=True)
    for f in [private,bg,host,header,view_sites,view_header,own/Path(__file__).name,imports,keys_path,private_builder,builder_path]:manifest['inputs'][str(f.resolve())]=background.sha(f)
    manifest.update(generated_sha256=background.sha(generated),binary_sha256=background.sha(binary),forest_extension={'active_control_frames':a.active_frames,'frame_limit':a.frames,'input':'right every active tick; button1 first2of8 ticks; existing ordinary coin/start only','target_products_unchanged':True,'maximum_unique_rejection_snapshots':33})
    manifest['frozen_source_aliases']={str(n):str(f) for n,f in aliases.items()}
    if body_source:
        manifest['inputs'][str(body_source.resolve())]=background.sha(body_source)
        manifest['inputs'][str(irq_header.resolve())]=background.sha(irq_header)
        manifest['forest_extension']['source_body_observation']={'entry':0x92ee,'completed_before_call':0x92f2,'original_body':0x93be,'opcode_guards':True,'host_target_writes':False}
    assert background.sha(a.core)==background.CORE_SHA
    with(out/'observer.log').open('w') as log:r=subprocess.run([str(binary),str(a.core),str(a.frames),'--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
    manifest['run']={'exit_code':r.returncode,'frames':a.frames,'core_sha256':background.sha(a.core),'log_sha256':background.sha(out/'observer.log')}
    (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    assert all(background.sha(Path(n))==d for n,d in manifest['inputs'].items()),'Frozen dependency changed during extension'
    offset=0;regions=[]
    for name,size,address in REGIONS:regions.append({'name':name,'offset':offset,'size':size,'source_start':address});offset+=size
    layout={'source_context_bytes':offset,'regions':regions,'objects':{'record_bytes':4096,'banks':[{'offset':0,'source_start':0x900000,'bytes':2048},{'offset':2048,'source_start':0x908000,'bytes':2048}]},'byte_order':'canonical big endian','context_boundary':'actual reference-path rejection, followed by strict paired-frame completion when coverage-stop.json exists'}
    (out/'rejection-layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    frames=list(csv.DictReader((out/'frames.csv').open())) if(out/'frames.csv').exists() else []
    report={'scope':__doc__,'exit_code':r.returncode,'paired_frames_logged':len(frames),'last_route_tick':int(frames[-1]['route_tick']) if frames else 0,'requested_active_control_frames':a.active_frames,'source_register_failures':len(list(csv.DictReader((out/'register-failures.csv').open()))) if(out/'register-failures.csv').exists() else 0,'target_admitted':False,'stopped_on_confirmed_coverage_boundary':(out/'coverage-stop.json').exists(),'rejection_captures':[json.loads(f.read_text()) for f in sorted(out.glob('rejection-???.json'))]}
    if(out/'coverage-stop.json').exists():report['paired_stop']=json.loads((out/'coverage-stop.json').read_text());assert r.returncode==3
    (out/'extension-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(r.returncode)


if __name__=='__main__':main()
