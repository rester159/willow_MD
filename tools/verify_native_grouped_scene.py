#!/usr/bin/env python3
"""Compile prepared grouped templates; measure native geometry/Window/SAT.

This is a headless static source-verified integration proof, not gameplay.
Immutable assets are prepared before the timed publication, which refreshes
origins from a full original bank and constructs every SAT word on the 68000.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
from PIL import Image
from build_vdp_presentation_probe import ROOT, build


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def array(name, words, align=''):
    rows = [','.join(hex(w) for w in words[n:n+16])+',' for n in range(0,len(words),16)]
    return f'static const uint16_t {name}[] {align}={{\n'+ '\n'.join(rows)+'\n};\n'


def verify(fixture, out, core, native_commit=False, native_cells=False):
    fixture,out,core=fixture.resolve(),out.resolve(),core.resolve(strict=True)
    if fixture==out or out.is_relative_to(fixture):
        raise ValueError('Keep output separate from immutable fixture')
    out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((fixture/'manifest.json').read_text())
    assert manifest['source_composition_pixels_verified']==71680
    assert manifest['source_composition_mismatches']==0
    for name,info in manifest['buffers'].items():
        assert (fixture/name).stat().st_size==info['bytes'] and sha(fixture/name)==info['sha256']
    group=json.loads((fixture/'group-plan.json').read_text())
    for name,digest in group['hashes'].items():
        assert sha(Path(name))==digest, f'Grouped preparation input changed: {name}'
    source_path=ROOT/'build/native-prepared-objects/final1200/prepared-plan.json'
    source=json.loads(source_path.read_text())
    binding_path=ROOT/'build/cartridge-gap-audit/frame1200_window_v1/asset-binding.json'
    binding=json.loads(binding_path.read_text())
    capture_path=ROOT/binding['source_capture']
    assert sha(capture_path)==binding['source_capture_sha256']
    raw=capture_path.read_bytes()[binding['bank_offset']:binding['bank_offset']+2048]
    assert len(raw)==2048 and hashlib.sha256(raw).hexdigest()==binding['bank_sha256']
    assert source['source_records']==256 and group['instances']==source['instances']
    assert len(group['templates'])==len(source['templates'])==3
    vram=(fixture/'vram.bin').read_bytes()
    count=group['new_sat_entries']
    assert count==13 and manifest['sat_entries']==count
    assert source['view']==[96,16,320,224,0]
    header='/* Source-derived immutable resource descriptors; positions reread natively. */\n'
    for name,value in [('RECORD_COUNT',256),('OBJECT_COUNT',count),
                       ('MAX_SPRITES',group['max_line_sprites']),('MAX_PIXELS',group['max_line_pixels'])]:
        header+=f'#define FIXTURE_{name} {value}\n'
    header+=array('fixtureBank',struct.unpack('>1024H',raw),'__attribute__((aligned(4)))')
    header+=array('fixtureExpected',struct.unpack('>'+str(count*4)+'H',vram[0xf400:0xf400+count*8]))
    window=struct.unpack('>2048H',vram[0xd000:0xe000])
    assert all(w==0 for n,w in enumerate(window) if n//64>=28 or n%64>=40)
    header+=array('fixtureWindow',[window[y*64+x] for y in range(28) for x in range(40)])
    header+=array('fixtureColors',struct.unpack('>64H',(fixture/'cram.bin').read_bytes()))
    for n,(cells,original) in enumerate(zip(group['templates'],source['templates'])):
        records=[]
        for cell in cells:
            offset=(cell['y']<<8)|cell['x']|(cell['size_word']>>8)
            records.append('{'+f'{offset},{cell["attribute"]}'+'}')
        header+=f'static const WillowGroupedObjectCell cells{n}[]={{'+','.join(records)+'};\n'
        header+=f'static const WillowGroupedObjectTemplate template{n}={{cells{n},{len(cells)},{original["base"]},{original["attribute"]}}};\n'
    header+=f'static WillowGroupedObjectInstance instances[{len(group["instances"])}];\n'
    header+='static WillowGroupedObjectFrame fixtureFrame;\nstatic WillowGroupedObjectBinding fixtureBinding;\nstatic WillowGroupedObjectEpoch fixtureEpoch;\n'
    header+='static uint8_t fixtureInit(void){\n'
    for n,instance in enumerate(group['instances']):
        header+=f'instances[{n}].shape=&template{instance["template"]};instances[{n}].sourceX=0;instances[{n}].sourceY=0;instances[{n}].sourceOrdinal={instance["ordinal"]};\n'
    header+='fixtureEpoch.latch=1;fixtureEpoch.resident=77;fixtureEpoch.palette=77;fixtureEpoch.view=1;\n'
    header+='fixtureFrame.instances=instances;fixtureFrame.count=3;fixtureFrame.sourceRecords=256;fixtureFrame.epoch=fixtureEpoch;fixtureFrame.epoch.latch=0;\n'
    header+='fixtureFrame.view.left=96;fixtureFrame.view.top=16;fixtureFrame.view.width=320;fixtureFrame.view.height=224;fixtureFrame.view.flipScreen=0;\nreturn willowGroupedObjectBind(&fixtureBinding,&fixtureFrame)==0;}\n'
    header+='static uint8_t fixtureLatch(const volatile uint8_t*ram){\nif(!willowGroupedObjectLatchBegin(&fixtureBinding))return 0;\nuint16_t words[4];\n'
    for instance in group['instances']:
        ordinal=instance['ordinal']
        header+=f'if(!willowNativeObjectLatchReadRecord(ram,{ordinal},words)||!willowGroupedObjectLatchOrigin(&fixtureBinding,{ordinal},words[0],words[1],words[2],words[3]))return 0;\n'
    header+='return willowGroupedObjectLatchSeal(&fixtureBinding,fixtureEpoch.latch);}\n'
    (out/'grouped_scene_fixture.h').write_text(header)
    initial=bytearray(vram)
    initial[0xd000:0xe000]=bytes(4096)
    initial[0xf400:0xf680]=bytes(640)
    inputs=out/'inputs';inputs.mkdir(exist_ok=True)
    for name in ['vram','cram','vsram','registers']:
        (inputs/(name+'.bin')).write_bytes(initial if name=='vram' else (fixture/(name+'.bin')).read_bytes())
    native_source=ROOT/'tools/native_grouped_scene.c'
    if native_commit or native_cells:
        native_source=out/'grouped_native.c'
        native_source.write_text(('#define WILLOW_GROUPED_NATIVE_COMMIT 1\n' if native_commit else '')+
                                 ('#define WILLOW_GROUPED_NATIVE_CELLS 1\n' if native_cells else '')+
                                 (ROOT/'tools/native_grouped_scene.c').read_text())
    build({name:inputs/(name+'.bin') for name in ['vram','cram','vsram','registers']},out,
          native_source=native_source)
    subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2',
        '-DWILLOW_PRESENTATION_PHASES','-DWILLOW_PRESENTATION_VRAM_DUMP',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all',
        str(ROOT/'tools/capture_vdp_presentation.c'),'-o',str(out/'capture')],check=True)
    with (out/'capture.log').open('w') as log:
        subprocess.run([str(out/'capture'),str(core),str(out/'probe.bin'),str(out/'hardware.ppm')],
                       stdout=log,stderr=subprocess.STDOUT,check=True)
    phases={}
    for line in (out/'capture.log').read_text().splitlines():
        if line.startswith('PHASE '):
            _,name,cycles,ms=line.split()
            phases[name]={'master_cycles':int(cycles),'ms':float(ms)}
    assert len(phases)==5
    actual=Image.open(out/'hardware.ppm').convert('RGB')
    expected=Image.open(fixture/manifest['expected_image']).convert('RGB')
    assert actual.size==expected.size==(320,224)
    target=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in expected.get_flattened_data()]
    bad=sum(a!=b for a,b in zip(actual.get_flattened_data(),target))
    actual.save(out/'hardware.png')
    actual_vram=(out/'hardware.ppm.vram').read_bytes()
    vram_bad=sum(a!=b for a,b in zip(actual_vram,vram))
    files=[Path(__file__),ROOT/'tools/native_grouped_scene.c',ROOT/'tools/capture_vdp_presentation.c',
           ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/prepare_native_sprite_groups.py',
           source_path,binding_path,capture_path,fixture/'manifest.json',fixture/'group-plan.json',
           out/'grouped_scene_fixture.h',out/'probe.bin',out/'capture.log',out/'hardware.ppm',
           out/'hardware.ppm.vram',core]
    if native_source!=ROOT/'tools/native_grouped_scene.c':
        files.append(native_source)
    files+=[ROOT/'src'/name for name in ['willow_native_object_grouped.h',
        'willow_native_object_sat_budget_m68k.h','willow_native_object_sat.h',
        'willow_native_object_latch.h','willow_native_window_surface.h',
        'willow_native_sat_publish.h','willow_native_presentation_plan.h']]
    report={'scope':'Static source-verified scene; actual native grouped geometry, full-bank origin refresh, Window cache and SAT publication. No live producer/resource integration, active-display/VBlank admission, game loop, audio or FPS proof.',
        'pixels':71680,'mismatches':bad,'native_vram_bytes':len(actual_vram),'vram_mismatches':vram_bad,
        'native_sat_words_exact':count*4,'old_sat_entries':group['old_sat_entries'],'native_sat_entries':count,
        'pattern_bytes':group['new_object_patterns']*32,'extra_pattern_bytes':0,
        'max_line_sprites':group['max_line_sprites'],'max_line_pixels':group['max_line_pixels'],
        'native_source_origin_refresh':True,'second_latch_epoch':2,'source_bank_bytes':2048,
        'cold_preparation_in_timed_phases':False,'expected_sat_is_assertion_only':True,
        'initial_window_and_sat_erased':True,'timing':phases,
        'native_scanline_commit':native_commit,
        'native_inside_cell_loop':native_cells,
        'steady_non_assertion_ms':sum(v['ms'] for k,v in phases.items() if k!='diagnostic_assertions'),
        'timing_scope':'Closed NTSC display-disabled cost; fixed resident assets and shared immutable palette/resource generation. Unchanged Window reused. CPU geometry and publisher timing, not live FPS.',
        'hashes':{str(p.resolve()):sha(p) for p in files}}
    (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    assert bad==0 and len(actual_vram)==65536 and vram_bad==0,report
    return report


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--fixture',type=Path,default=ROOT/'build/native-sprite-groups/frame1200')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-grouped-scene/frame1200')
    p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    p.add_argument('--native-commit',action='store_true')
    p.add_argument('--native-cells',action='store_true')
    args=p.parse_args()
    result=verify(args.fixture,args.output,args.core,args.native_commit,args.native_cells)
    print(json.dumps({k:result[k] for k in ['pixels','mismatches','native_vram_bytes','vram_mismatches','native_sat_entries','steady_non_assertion_ms','timing']},indent=2))
