#!/usr/bin/env python3
"""Read-only render/cadence observer composed onto pinned compact reference.

Only generated harness files change. Source/platform products belong to the
renderer builder. Both paths execute identical renderer instructions and VDP
work, while original source addresses retain the independent bus oracle.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess

ROOT=Path(__file__).resolve().parents[1]
PIN=ROOT/'build/native-object-publication/checkpoint-compact-v1'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def replace(text,before,after,count=1):
    assert text.count(before)==count,(before[:90],text.count(before),count)
    return text.replace(before,after)

def extra_rom_resource(platform, manifest, enabled):
    """Validate one common immutable ROM segment outside either program page."""
    product=manifest.get('extra_rom')
    if not enabled:
        assert product is None,'Manifest extra_rom requires explicit --extra-rom'
        return None
    assert isinstance(product,dict) and set(product)=={'file','origin','bytes','sha256'},'Invalid extra_rom metadata'
    name=product['file'];origin=product['origin'];size=product['bytes'];digest=product['sha256']
    assert isinstance(name,str) and name not in {'','.','..'} and Path(name).name==name and '\\' not in name and '\0' not in name,'extra_rom file must be a plain relative basename'
    assert type(origin) is int and type(size) is int and size>0,'Invalid extra_rom range types or empty segment'
    assert origin>=0x110000 and not origin&1 and origin+size<=0x200000,'extra_rom overlaps source/platform pages or SRAM'
    assert isinstance(digest,str) and re.fullmatch('[0-9a-f]{64}',digest),'Invalid extra_rom SHA256'
    path=(platform/name).resolve()
    assert path.parent==platform.resolve() and path.is_file(),'extra_rom file escapes platform directory or is missing'
    data=path.read_bytes()
    assert len(data)==size and hashlib.sha256(data).hexdigest()==digest,'extra_rom bytes/SHA256 mismatch'
    return {'path':path,'origin':origin,'bytes':size,'sha256':digest,'data':data}

def retained_oracle(platform, out, manifest, before):
    """Prepare host-only numeric reference assets; never embedded in the ROM."""
    paths=[Path(name) for name in manifest['resource_files'] if name.endswith('/route_objects.h')]
    assert len(paths)==1, paths
    assets=paths[0].parent
    resource_manifest=assets/'manifest.json'
    prepared=json.loads(resource_manifest.read_text())
    before[str(resource_manifest)]=sha(resource_manifest)
    for name in ['object-patterns.bin','source-palettes.bin','cram.bin']:
        path=assets/name
        assert sha(path)==prepared['outputs'][name]
        assert manifest['resource_files'][str(path)]==sha(path)
        before[str(path)]=sha(path)
    entries=prepared['entries']
    assert len({(e['code'],e['source_group']) for e in entries})==len(entries)
    output='/* Read-only host oracle, not target data or captured geometry. */\n'
    output+='typedef struct {unsigned code,group,status,attribute;} RenderOracleEntry;\n'
    output+='static const RenderOracleEntry renderOracleEntries[]={'+','.join(
        '{%d,%d,%d,%d}'%(e['code'],e['source_group'],e['status'],e['attribute']) for e in entries)+'};\n'
    output+=f'#define RENDER_ORACLE_FIRST_BYTE {prepared["first_tile"]*32}u\n'
    for name, symbol in [('object-patterns.bin','renderOraclePatterns'),('source-palettes.bin','renderOraclePalettes'),('cram.bin','renderOracleColors')]:
        data=(assets/name).read_bytes()
        output+=f'static const uint8_t {symbol}[]={{'+','.join(map(str,data))+'};\n'
    target=out/'render-oracle-assets.h';target.write_text(output)
    before[str(target)]=sha(target)
    return '#include "'+str(target)+'"\n'

def build(platform,out,extra_rom=False):
    platform,out=platform.resolve(),out.resolve()
    assert platform!=out,'Keep builder products and observer output separate'
    out.mkdir(parents=True,exist_ok=True)
    checkpoint=json.loads((PIN/'checkpoint.json').read_text())['files']
    original=PIN/'tools/test_native_cadence_compact_latch.c'
    assert sha(original)==checkpoint['tools/test_native_cadence_compact_latch.c']
    header=platform/'render-addresses.h'
    if not header.exists():header=platform/'platform-addresses.h'
    assert header.is_file()
    products=[platform/name for name in ['program-original.bin','program.bin','platform-original.bin','platform.bin']]
    assert all(p.is_file() for p in products)
    for p in products[2:]:assert p.stat().st_size<=65536-16,'Pinned carrier has one64KiB platform page; expand explicitly before larger images'
    before={str(p):sha(p) for p in [original,header,*products,Path(__file__),ROOT/'tools/test_native_render_cadence_observer.h',ROOT/'tools/test_native_render_cadence_oracle.h']}
    additions=''
    product_manifest=platform/'manifest.json'
    manifest_bytes=product_manifest.read_bytes() if product_manifest.is_file() else None
    manifest=json.loads(manifest_bytes) if manifest_bytes is not None else {}
    if manifest_bytes is not None:before[str(product_manifest)]=hashlib.sha256(manifest_bytes).hexdigest()
    extra=extra_rom_resource(platform,manifest,extra_rom)
    if extra:
        before[str(extra['path'])]=extra['sha256']
        # Compile the verified input into this host fixture, not into native
        # instructions. No unchecked build-to-run file replacement is possible.
        data=extra['data']
        payload='/* Verified immutable common ROM bytes, host fixture only. */\n'
        payload+=f'#define RENDER_EXTRA_ROM_ORIGIN {extra["origin"]}u\n'
        payload+=f'static const uint8_t renderExtraROM[{len(data)}]={{\n'
        payload+='\n'.join(','.join(map(str,data[n:n+32]))+',' for n in range(0,len(data),32))+'\n};\n'
        extra_header=out/'extra-rom-assets.h';extra_header.write_text(payload)
        before[str(extra_header)]=sha(extra_header)
        additions+='#include "'+str(extra_header)+'"\n'
    if manifest_bytes is not None:
        symbols=manifest.get('symbols',{})
        if manifest.get('retained_sat') or manifest.get('retained_sat_diff') or manifest.get('content_stream'):
            assert '#define RENDER_RETAINED_SAT 1' in header.read_text() or '#define RENDER_CONTENT_STREAM 1' in header.read_text()
            additions+=retained_oracle(platform,out,manifest,before)
            oracle=ROOT/'tools/test_native_render_cadence_oracle.h'
            before[str(oracle)]=sha(oracle)
        if 'nativeRenderPublish' in symbols:
            image=products[2].read_bytes();origin=0x100010
            start,end=symbols['__render_code_begin'],symbols['__render_code_end']
            retained=manifest.get('retained_geometry',manifest.get('retained_sat',False))
            sites=[]
            if retained:
                for source,target in [(0x900000,0x20c000),(0x908000,0x20d000)]:
                    pattern=bytes.fromhex('2d7c')+source.to_bytes(4,'big')+bytes.fromhex('ffac')
                    choices=[at for at in range(start,end-7,2) if image[at-origin:at-origin+8]==pattern]
                    assert len(choices)==1,choices
                    sites.append((choices[0],8,1,16,-84,0,0,pattern))
                pattern=bytes.fromhex('2d6effacffde')
                choices=[at for at in range(start,end-5,2) if image[at-origin:at-origin+6]==pattern]
                assert len(choices)==1,choices
                sites.append((choices[0],6,2,17,-34,-84,0,pattern))
            else:
                pattern=bytes.fromhex('2d46ffde')
                spills=[at for at in range(start,end-3,2) if image[at-origin:at-origin+4]==pattern]
                assert len(spills)==1,spills
                sites.append((spills[0],4,0,6,-34,0,0,pattern))
            for function in ['willowPreparedObjectBind.constprop.0','nativeRenderPublish']:
                entry=symbols[function]
                choices=[at for at in range(entry,entry+16,2) if image[at-origin:at-origin+4]==bytes.fromhex('2f0a2f02')]
                if not choices and function=='nativeRenderPublish':
                    movems=[at for at in range(entry,entry+16,2) if image[at-origin:at-origin+2]==bytes.fromhex('48e7')]
                    assert len(movems)==1,('Unproved publisher prologue',hex(entry))
                    continue
                assert len(choices)==1,(function,hex(entry));at=choices[0]
                sites.extend([(at,2,0,10,0,0,int(function=='nativeRenderPublish'),bytes.fromhex('2f0a')),
                              (at+2,2,0,2,0,0,int(function=='nativeRenderPublish'),bytes.fromhex('2f02'))])
            candidate=products[3].read_bytes()
            additions+='typedef struct {unsigned pc,size,kind,reg;int displacement,sourceDisplacement;unsigned publishing;} RenderPointerStoreSite;\n'
            additions+='static const RenderPointerStoreSite renderPointerStoreSites[]={\n'
            for at,size,kind,reg,disp,source_disp,publishing,pattern in sites:
                expected=pattern
                if kind==1:
                    value=int.from_bytes(pattern[2:6],'big');target={0x900000:0x20c000,0x908000:0x20d000}[value]
                    expected=pattern[:2]+target.to_bytes(4,'big')+pattern[6:]
                assert candidate[at-origin:at-origin+size]==expected
                additions+='{%du,%du,%du,%du,%d,%d,%du},\n'%(at,size,kind,reg,disp,source_disp,publishing)
            additions+='};\n#define RENDER_POINTER_STORE_COUNT (sizeof renderPointerStoreSites/sizeof *renderPointerStoreSites)\n'
    text=original.read_text()
    text=replace(text,'#include "vendor/libretro.h"','#include "'+str(ROOT/'tools/vendor/libretro.h')+'"')
    text=replace(text,'#include "../src/willow_arcade_player_motion.h"','#include "'+str(ROOT/'src/willow_arcade_player_motion.h')+'"')
    text=replace(text,'#include "../build/native-object-publication/compact-v1/platform-addresses.h"','#include "'+str(header)+'"\n'+additions)
    text=replace(text,'static unsigned sourcePC(void);','static unsigned sourcePC(void);\nstatic void renderBeforeWork(unsigned,unsigned,unsigned);\nstatic void renderAfterWork(unsigned,unsigned,unsigned);\nstatic void renderSelect(unsigned);\nstatic unsigned renderNativeRead8(unsigned);\nstatic unsigned renderAllowedInterruptPC(unsigned);\nstatic void renderPaletteWrite(unsigned,unsigned);\nstatic unsigned renderPointerStoreOwner(unsigned);\nstatic unsigned renderPointerStoreDetails(unsigned,unsigned,unsigned*,unsigned*,unsigned*);\nstatic void renderCheckSourceContext(unsigned);\nstatic unsigned renderExceptionStore(unsigned,unsigned,unsigned);')
    text=replace(text,'static void sourceWorkAccess(unsigned at,unsigned width){','static void sourceWorkAccess(unsigned at,unsigned width){\n for(unsigned n=0;n<width;n++)(void)renderNativeRead8(at+n);')
    text=replace(text,'static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){','static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){\n renderBeforeWork(at,width,value);')
    text=replace(text,'static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){','static void headroomAfterWrite(unsigned at,unsigned width,unsigned value){\n renderAfterWork(at,width,value);')
    text=replace(text,'savedPC>=0x100000','!renderAllowedInterruptPC(savedPC)')
    # A genuine IRQ may arrive between the original wait's MOVEM and TRAP.
    # Keep its pending logical begin until commit; the old interval still has
    # no commit, so the independent deadline/missing-update counters fail it.
    text=replace(text,'  waitBegin=waitCommit=0;windowMainWaits=0;','  waitBegin=waitCommit?0:waitBegin;waitCommit=0;windowMainWaits=0;')
    text=replace(text,'videoBounds(at,1);publicationVideoWrite(at,1,v);','videoBounds(at,1);renderPaletteWrite(at,1);publicationVideoWrite(at,1,v);')
    text=replace(text,'videoBounds(at,2);if(at&1)abort();publicationVideoWrite(at,2,v);','videoBounds(at,2);if(at&1)abort();renderPaletteWrite(at,2);publicationVideoWrite(at,2,v);')
    text=replace(text,' if(at==0x800100&&latchCaptures){',' if(at==0x800100&&latchCaptures){\n  renderCheckSourceContext(v);')
    text=replace(text,' return 0;\n}\nstatic void workWrite8',' return renderPointerStoreOwner(next);\n}\nstatic void workWrite8')
    text=replace(text,'  if(!found||(reg!=15','  if(!found)found=renderPointerStoreDetails(pc,at,&reg,&index,&value);\n  if(!found||(reg!=15')
    text=replace(text,'unsigned pc=storeOwner();owner[at]=owner[at+1]=pc;', 'unsigned pc=storeOwner();if(renderExceptionStore(pc,at,v))pc=0;owner[at]=owner[at+1]=pc;')
    text=replace(text,'  return; /* Reference game RAM retains source bytes; native capture is checked independently. */','  ram[at^1]=(uint8_t)(v>>8);ram[(at+1)^1]=(uint8_t)v;\n  return; /* Materialize independently checked capture for the real reference renderer. */')
    text=replace(text,' }\n}\nstatic int samePointer',' }\n renderSelect(path);\n}\nstatic int samePointer')
    text=replace(text,'int main(int argc,char**argv){','#include "'+str(ROOT/'tools/test_native_render_cadence_observer.h')+'"\nint main(int argc,char**argv){')
    text=text.replace('build/native-object-publication/compact-v1/',str(platform)+'/')
    text=replace(text,'static const char*outputDirectory="build/native-object-publication/compact-v1";','static const char*outputDirectory='+json.dumps(str(out))+';')
    text=replace(text,' if(m68k_get_reg(16)!=CADENCE_BOOT',' if(!renderInit(core))return 2;\n if(m68k_get_reg(16)!=CADENCE_BOOT')
    text=replace(text,'   retro_run();','   retro_run();\n   if(renderObserveFrame(path))return 1;')
    text=replace(text,' free(states[0]);free(states[1]);return 0;',' renderFinish();\n free(states[0]);free(states[1]);return 0;')
    if extra:
        # Both path-specific platform images end at10FFFF. The core receives
        # this common segment in the original ROM image before retro_load_game;
        # selectProgram only replaces pages00..10 and preserves pages11..1F.
        text=replace(text,' char oraclePath[512];',' memcpy(rom+RENDER_EXTRA_ROM_ORIGIN,renderExtraROM,sizeof renderExtraROM);\n char oraclePath[512];')
        check=''' renderSelect(path);
 {static unsigned checked[2];if(!checked[path]){
  for(unsigned n=0;n<sizeof renderExtraROM;n++){
   unsigned address=RENDER_EXTRA_ROM_ORIGIN+n;const uint8_t*page=cpu->map[address>>16].base;
   if(!page||page[(address&65535)^1]!=renderExtraROM[n]){fprintf(stderr,"Common extra ROM mapping mismatch at%x path%u\\n",address,path);abort();}
  }
  checked[path]=1;
 }}
}
static int samePointer'''
        text=replace(text,' renderSelect(path);\n}\nstatic int samePointer',check)
    generated=out/'test_native_render_cadence.c';generated.write_text(text)
    binary=out/'test-native-render-cadence'
    command=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror',str(generated),'-o',str(binary)]
    subprocess.run(command,cwd=ROOT,check=True)
    assert all(sha(Path(n))==v for n,v in before.items()),'Dependency changed during observer build'
    manifest={'scope':__doc__,'inputs':before,'generated_sha256':sha(generated),'binary_sha256':sha(binary),'command':command,'platform':str(platform),'output':str(out)}
    if extra:manifest['extra_rom']={'file':str(extra['path']),'origin':extra['origin'],'bytes':extra['bytes'],'sha256':extra['sha256'],'host_embedded':True,'core_path_mapping_assertions':True}
    (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return binary,manifest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform',type=Path,default=ROOT/'build/native-render-cadence/v1')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-render-cadence/observer-v1')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--frames',type=int,default=1800)
    p.add_argument('--build-only',action='store_true')
    p.add_argument('--extra-rom',action='store_true',help='Load the manifest-verified common immutable ROM segment at110000..1FFFFF')
    a=p.parse_args();assert 1<=a.frames<=36000
    binary,manifest=build(a.platform,a.output,a.extra_rom)
    if not a.build_only:
        with(a.output/'observer.log').open('w')as log:
            result=subprocess.run([str(binary),str(a.core),str(a.frames),'--coin-route',str(a.output.resolve())],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        manifest['run']={'exit_code':result.returncode,'frames':a.frames,'core_sha256':sha(a.core),'log_sha256':sha(a.output/'observer.log')}
        (a.output/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print(json.dumps(manifest['run'],indent=2))
        if result.returncode:raise SystemExit(result.returncode)
    print(binary)

if __name__=='__main__':main()
