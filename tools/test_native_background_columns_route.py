#!/usr/bin/env python3
"""Declared columns target forest observation with exact stack provenance.

Composes the strict forest observer. No target/state patches, skipped RAM bytes,
or reduced source/VDP comparison. Diagnostics are read-only.
"""
from pathlib import Path
import sys
import types
import json
import struct
import test_native_forest_extension as forest

ROOT=Path(__file__).resolve().parents[1]
replace=forest.background.cadence.replace


def pointer_sites(platform,target,text,out):
    origin=0x100010
    images=[(platform/n).read_bytes() for n in ['platform-original.bin','platform.bin']]
    begin=target['symbols']['nativeBackgroundPrepare']
    end=min(v for v in target['symbols'].values() if v>begin)
    sites=[]
    for source,native in [(0x914000,0x208000),(0x918000,0x204000)]:
        opcode=b'\x2d\x7c'+struct.pack('>Ih',source,-134)
        converted=b'\x2d\x7c'+struct.pack('>Ih',native,-134)
        matches=[at for at in range(begin,end-7,2) if images[0][at-origin:at-origin+8]==opcode]
        assert len(matches)==1,('Unreviewed source column base spill',matches)
        pc=matches[0]
        assert images[1][pc-origin:pc-origin+8]==converted,'Native immediate differs from exact relocation'
        sites.append({'pc':pc,'next_pc':pc+8,'size':8,'kind':1,'reg':16,'displacement':-134,
                      'source':source,'native':native,'original_opcode':opcode.hex(),'native_opcode':converted.hex()})
    insertion=''.join('{%du,8u,1u,16u,-134,0,0u},\n'%v['pc'] for v in sites)
    text=replace(text,'static const RenderPointerStoreSite renderPointerStoreSites[]={\n',
                 'static const RenderPointerStoreSite renderPointerStoreSites[]={\n'+insertion)
    (out/'columns-pointer-sites.json').write_text(json.dumps({
        'scope':'Exact two compiler immediate stores; existing dynamic A6+displacement/native stack bounds/word value checks retained; no memory comparison skipped',
        'sites':sites,'diagnostic':'columns-v1-stack-diagnostic-v2 proves actual callback PC=site+8 for both high/low words'},indent=2)+'\n')
    return text


def exception_provenance(header):
    # The caller's existing proof still requires supervisor FFF000..FFFFFA,
    # current A7, hardware level6, all three PC/SR stores, and actual IRQ entry.
    # A normal prologue push is in the declared native stack and cannot qualify.
    extra=''' if(pc==ownerPC+2&&ownerPC>=0x100000&&ownerPC+2<=0x110000&&renderAllowedInterruptPC(ownerPC)){
  for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++){
   const RenderPointerStoreSite*s=renderPointerStoreSites+n;
   if(s->pc!=ownerPC||s->size!=2||s->kind!=0||s->reg>14)continue;
   unsigned at=ownerPC-0x100000;
   if(willowMotionWord(platforms[0]+at)==(0x2f00u|s->reg)&&!memcmp(platforms[0]+at,platforms[1]+at,2))return 1;
  }
 }
'''
    text=header.read_text()
    text=replace(text,'static unsigned backgroundInterruptedMovem(unsigned ownerPC,unsigned pc){',
                 'static unsigned backgroundInterruptedMovem(unsigned ownerPC,unsigned pc){\n'+extra)
    header.write_text(text)


def main():
    diagnostic='--diagnose-stack' in sys.argv
    if diagnostic:sys.argv.remove('--diagnose-stack')
    original=ROOT/'tools/test_native_forest_extension.py'
    text=original.read_text()
    before="    module=types.ModuleType('forest_background_builder');module.__file__=str(builder_path)"
    after='''    # Read exact frozen target sources; keep the original manifest digest.
    builder_text=background.cadence.replace(builder_text,
        "  data=original.read_bytes();assert hashlib.sha256(data).hexdigest()==digest,('Target source changed before snapshot',name)",
        "  frozen=platform/'frozen-sources'/original.relative_to(ROOT)\\n  data=frozen.read_bytes();assert hashlib.sha256(data).hexdigest()==digest,('Target frozen source changed before snapshot',name)")
'''+before
    text=replace(text,before,after)
    if not diagnostic:
        text=replace(text,'    bg.write_text(text)','    bg.write_text(text)\n    columns_exception_provenance(bg)')
        before="    generated.write_text(text);subprocess.run(manifest['command'],cwd=ROOT,check=True)"
        text=replace(text,before,'    text=columns_pointer_sites(platform,target,text,out)\n'+before)
    if diagnostic:
        before="    generated.write_text(text);subprocess.run(manifest['command'],cwd=ROOT,check=True)"
        after='''    text=background.cadence.replace(text,'static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){',
        'static void headroomBeforeWrite(unsigned at,unsigned width,unsigned value){\\n if(frameNumber>=1355&&at>=0x74c8&&at<=0x74ce)fprintf(stderr,"Column stack trace frame%u pc%x at%x width%u value%x A0%x A6%x SP%x\\\\n",frameNumber,sourcePC(),at,width,value,cpu->dar[8],cpu->dar[14],cpu->dar[15]);')
'''+before
        text=replace(text,before,after)
    before="    own=out/'frozen-observer-tools';header=own/'test_native_forest_extension.h';"
    after="    own=out/'frozen-observer-tools';(own/'test_native_background_columns_route.py').write_bytes((ROOT/'tools/test_native_background_columns_route.py').read_bytes());header=own/'test_native_forest_extension.h';"
    text=replace(text,before,after)
    before='    assert background.sha(a.core)==background.CORE_SHA'
    after="    manifest['inputs'][str((own/'test_native_background_columns_route.py').resolve())]=background.sha(own/'test_native_background_columns_route.py')\n"
    if not diagnostic:after+="    manifest['inputs'][str((out/'columns-pointer-sites.json').resolve())]=background.sha(out/'columns-pointer-sites.json')\n"
    text=replace(text,before,after+before)
    module=types.ModuleType('columns_forest_observer');module.__file__=str(original)
    module.columns_pointer_sites=pointer_sites;module.columns_exception_provenance=exception_provenance
    exec(compile(text,str(original),'exec'),module.__dict__)
    module.main()

if __name__=='__main__':main()
