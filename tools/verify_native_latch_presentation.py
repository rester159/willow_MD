#!/usr/bin/env python3
"""Publish captured objects using the native scattered-latch SAT consumer.

Captured source records form the diagnostic input; the unused bank tail is
zero-padded after a terminator. The separate live cadence test proves physical
bank selection/latching. This static test proves native consumer integration.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
from PIL import Image
from build_vdp_presentation_probe import ROOT, build

FIXTURE=ROOT/'build/cartridge-gap-audit/frame1020_reuse_p3_w16_v1'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixture',type=Path,default=FIXTURE)
parser.add_argument('--asset-binding',type=Path)
parser.add_argument('--output',type=Path,default=ROOT/'build/native-latch-presentation')
parser.add_argument('--optimization',choices=['O2','Os','O3'],default='O2')
parser.add_argument('--deferred-budget',action='store_true')
args=parser.parse_args()
FIXTURE=args.fixture.resolve()
OUT=args.output.resolve()
if OUT==FIXTURE or OUT.is_relative_to(FIXTURE):
    raise ValueError('keep outputs outside the frozen source fixture')
OUT.mkdir(exist_ok=True,parents=True)
manifest=json.loads((FIXTURE/'manifest.json').read_text())
assert manifest['source_composition_pixels_verified']==71680
assert manifest['source_composition_mismatches']==0
for name,info in manifest['buffers'].items():
    blob=(FIXTURE/name).read_bytes()
    assert len(blob)==info['bytes'] and hashlib.sha256(blob).hexdigest()==info['sha256']
vram=(FIXTURE/'vram.bin').read_bytes()
assets={}
asset_files=[]
if args.asset_binding:
    binding_path=args.asset_binding.resolve()
    binding=json.loads(binding_path.read_text())
    capture_path=ROOT/binding['source_capture']
    raw=capture_path.read_bytes()
    assert hashlib.sha256(raw).hexdigest()==binding['source_capture_sha256']
    assert binding['bank_bytes']==2048 and binding['bank_offset'] in (0,0x8000)
    raw=raw[binding['bank_offset']:binding['bank_offset']+2048]
    assert len(raw)==2048 and hashlib.sha256(raw).hexdigest()==binding['bank_sha256']
    words=list(struct.unpack('>1024H',raw))
    for entry in binding['entries']:
        key=(entry['code']<<5)|entry['source_group']
        assert key not in assets and 0<=entry['source_group']<32
        assert entry['status'] in (1,2) and not(entry['attribute']&0x1800)
        assets[key]=(entry['attribute'],entry['status'])
    text_count=binding['text_sat_entries']
    asset_files=[binding_path,capture_path]
    bank_tail='all2048 original captured bytes, including markers and unused tail'
else:
    atlas_path=ROOT/'build/cartridge-gap-audit/object_palette_reuse_p3_w16.json'
    atlas_blob_path=atlas_path.with_suffix('.bin')
    atlas=json.loads(atlas_path.read_text())
    atlas_blob=atlas_blob_path.read_bytes()
    lookup={(entry['code'],entry['source_group']):entry for entry in atlas['entries']}
    object_area=next(a for a in manifest['ownership'] if a['kind']=='objects')
    resident={vram[tile*32:(tile+4)*32]:tile for tile in
              range(object_area['first_tile'],object_area['first_tile']+object_area['tiles'],4)}
    for x,y,base,attr in manifest['source_object_records']:
        for row in range((attr>>12)+1):
            for col in range(((attr>>8)&15)+1):
                code=(base&0xfff0)+((base+col)&15)+row*16
                entry=lookup.get((code,attr&31))
                if entry is None:
                    continue # Unavailable records reject if native geometry needs them.
                block=entry['variant_block']
                if not block:
                    assets[(code<<5)|(attr&31)]=(0,2)
                    continue
                raw=atlas_blob[block*128:(block+1)*128]
                if raw in resident:
                    page=atlas['maps'][entry['map']]['page']
                    assets[(code<<5)|(attr&31)]=(0x8000|(page<<13)|resident[raw],1)
    text_count=manifest['text_sat_entries']
    words=[w for record in manifest['source_object_records'] for w in record]+[0,0,0,0xff00]
    words += [0]*(1024-len(words))
    asset_files=[atlas_path,atlas_blob_path]
    bank_tail='diagnostic terminator and zero padding, not a full captured physical bank'
object_count=manifest['sat_entries']-text_count
record_count=next((n for n in range(256) if words[n*4+3]&0xff00==0xff00),256)
text=struct.unpack('>'+str(text_count*4)+'H',vram[0xf400:0xf400+text_count*8])
expected=struct.unpack('>'+str(object_count*4)+'H',vram[0xf400+text_count*8:0xf400+(text_count+object_count)*8])
def array(name,values,align=''):
    return f'static const uint16_t {name}[] {align}={{'+','.join(hex(n) for n in values)+'};\n'
header='#include <stdint.h>\n'
if args.deferred_budget:
    header+='#define WILLOW_DEFERRED_OBJECT_BUDGET 1\n'
for name,value in [('TEXT_COUNT',text_count),('OBJECT_COUNT',object_count),
                   ('RECORD_COUNT',record_count),
                   ('MAX_SPRITES',manifest['max_line_sprites']),('MAX_PIXELS',manifest['max_line_pixels'])]:
    header+=f'#define FIXTURE_{name} {value}\n'
header+=array('fixtureBank',words,'__attribute__((aligned(4)))')
if text_count:header+=array('fixtureText',text)
header+=array('fixtureExpected',expected)
header+='static const struct {uint32_t key;uint16_t attribute,status;} fixtureAssets[]={\n'
header+=''.join(f'{{{key},{attribute},{status}}},\n' for key,(attribute,status) in sorted(assets.items()))+'};\n'
(OUT/'latch_fixture.h').write_text(header)

# Objects are absent from the boot upload. Only the native consumer can restore
# them; expected SAT words are retained solely as an assertion after conversion.
initial=bytearray(vram)
initial[0xf400+text_count*8:0xf400+80*8]=bytes((80-text_count)*8)
input_dir=OUT/'inputs';input_dir.mkdir(exist_ok=True)
for name in ['vram','cram','vsram','registers']:
    (input_dir/(name+'.bin')).write_bytes(initial if name=='vram' else (FIXTURE/(name+'.bin')).read_bytes())
build({name:input_dir/(name+'.bin') for name in ['vram','cram','vsram','registers']},OUT,
      native_source=ROOT/'tools/native_latch_presentation.c',native_optimization='-'+args.optimization)
core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
subprocess.run(['clang','-Wall','-Wextra','-Werror','-O2','-DWILLOW_PRESENTATION_PHASES','-fsanitize=address,undefined',
                '-fno-sanitize-recover=all',str(ROOT/'tools/capture_vdp_presentation.c'),'-o',str(OUT/'capture')],check=True)
with (OUT/'capture.log').open('w') as log:
    subprocess.run([str(OUT/'capture'),str(core),str(OUT/'probe.bin'),str(OUT/'hardware.ppm')],stdout=log,stderr=subprocess.STDOUT,check=True)
phases={}
for line in (OUT/'capture.log').read_text().splitlines():
    if line.startswith('PHASE '):
        _,name,cycles,ms=line.split()
        phases[name]={'master_cycles':int(cycles),'ms':float(ms)}
assert len(phases)==5
actual=Image.open(OUT/'hardware.ppm').convert('RGB')
source=Image.open(FIXTURE/manifest['expected_image']).convert('RGB')
assert actual.size==source.size==(320,224)
target=[((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31) for r,g,b in source.get_flattened_data()]
bad=sum(a!=b for a,b in zip(actual.get_flattened_data(),target))
actual.save(OUT/'hardware.png')
files=[Path(__file__).resolve(),ROOT/'tools/native_latch_presentation.c',
       ROOT/'tools/build_vdp_presentation_probe.py',ROOT/'tools/capture_vdp_presentation.c',
       ROOT/'src/willow_native_object_sat.h',ROOT/'src/willow_native_object_latch.h',
       ROOT/'src/willow_native_object_latch_sat.h',FIXTURE/'manifest.json',*asset_files,
       OUT/'latch_fixture.h',OUT/'probe.bin',OUT/'hardware.ppm',OUT/'capture.log',core]
report={'scope':'Native static object consumer/publication with fixed resident assets; not live game, latch timing, DMA budget or FPS admission',
        'source_frame':manifest['frame'],'source_actor_frame':manifest.get('source_actor_frame'),'source_records':record_count,
        'bank_tail':bank_tail,
        'native_object_entries':object_count,'native_text_entries':text_count,
        'native_sat_words_exact':object_count*4,'pixels':71680,'mismatches':bad,
        'deferred_budget':args.deferred_budget,
        'timing':phases,
        'timing_scope':'Closed static NTSC diagnostic with display disabled and resident assets; marker-to-marker CPU costs only. Upload timing excludes active-display contention. This is not a live frame rate.',
        'asset_callback':'ROM binary search by original code/palette group; no geometry in lookup',
        'hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
(OUT/'evidence.json').write_text(json.dumps(report,indent=2)+'\n')
assert bad==0,report
print(f'Native scattered latch -> {object_count} original object SAT entries -> VDP: all71680 pixels match. No prebuilt object SAT upload, GUI, or host target writes.')
