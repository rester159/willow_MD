#!/usr/bin/env python3
"""Combine native prepared objects and Window cache in one headless VDP carrier.

This is a source-verified static scene with a cold preparation and measured
steady publication, not live gameplay. All source origins are reread natively.
"""
from pathlib import Path
import argparse
import hashlib
import json
import runpy
import struct
import subprocess
import sys
import build_vdp_presentation_probe as probe

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--native-cells', action='store_true')
p.add_argument('--output', type=Path, default=ROOT / 'build/native-prepared-scene/frame1200')
args = p.parse_args()
OUT = args.output.resolve()
FIXTURE = ROOT / 'build/cartridge-gap-audit/frame1200_window_v1'
CORE = Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
original_build = probe.build


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


def combined_build(inputs, out, **kwargs):
    out = Path(out)
    source_path = kwargs['native_source']
    assert source_path == out / 'prepared_native.c'
    header_path = out / 'prepared_fixture.h'
    header = replace_once(header_path.read_text(),
                          'static const WillowPreparedObjectEpoch preparedExpected={1,77,77,1};',
                          'static WillowPreparedObjectEpoch preparedExpected;')
    header = replace_once(header, 'willowPreparedObjectLatchSeal(&preparedBinding,1)',
                          'willowPreparedObjectLatchSeal(&preparedBinding,preparedExpected.latch)')
    header_path.write_text(header)
    source = source_path.read_text()
    source = replace_once(source, 'uint16_t nativeObjectPresentation(void)',
                          '#include "native_prepared_scene.h"\n\nuint16_t nativeObjectPresentation(void)')
    source = replace_once(source,
                          'if(!preparedFixtureInit()||!willowPreparedObjectSeedEmpty(&preparedSeed,1))return 0;',
                          'if(!combinedColdPrepare())return 0;')
    source = replace_once(source, ' *(volatile uint16_t*)0xff0004=4;',
                          ' if(!combinedSteadyPrepare())return 0;\n *(volatile uint16_t*)0xff0004=4;')
    first = source.index(' const uint32_t address=0xf400+FIXTURE_TEXT_COUNT*8;')
    last = source.index(' *(volatile uint16_t*)0xff0004=6;', first)
    source = source[:first] + ' if(!willowNativeSatPublish(&combinedSat,staged,result.pieces,&combinedFrame))return 0;\n' + source[last:]
    source_path.write_text(source)
    full_vram = (FIXTURE / 'vram.bin').read_bytes()
    map_words = struct.unpack('>2048H', full_vram[0xd000:0xe000])
    values = [map_words[y*64+x] for y in range(28) for x in range(40)]
    assert all(v == 0 for i, v in enumerate(map_words) if i//64 >= 28 or i%64 >= 40)
    colors = struct.unpack('>64H', (FIXTURE / 'cram.bin').read_bytes())
    native = '/* Immutable prepared native words, source-verified frame1200. */\n'
    for name, words in [('combinedWords', values), ('combinedColors', colors)]:
        native += f'static const uint16_t {name}[]={{\n'
        native += '\n'.join(','.join(str(w) for w in words[n:n+16])+',' for n in range(0, len(words), 16))
        native += '\n};\n'
    (out / 'combined_fixture.h').write_text(native)
    # The normal object carrier already erases the object SAT. Erase Window
    # too, so both actual native consumers must publish the final scene.
    vram = bytearray(inputs['vram'].read_bytes())
    vram[0xd000:0xe000] = bytes(4096)
    altered = out / 'inputs/combined-vram.bin'
    altered.write_bytes(vram)
    inputs = dict(inputs, vram=altered)
    # Includes resolve relative to this generated C file and tools/.
    (out / 'native_prepared_scene.h').write_bytes((ROOT / 'tools/native_prepared_scene.h').read_bytes())
    return original_build(inputs, out, **kwargs)


probe.build = combined_build
sys.argv = [str(ROOT / 'tools/verify_native_prepared_object_presentation.py'),
            '--certified-empty', '--output', str(OUT)]+(['--native-cells'] if args.native_cells else [])
runpy.run_path(str(ROOT / 'tools/verify_native_prepared_object_presentation.py'), run_name='__main__')

# Independently observe actual final VRAM, including pixels outside the visible
# Window. The native program is run unchanged, and the host only reads memory.
capture = (ROOT / 'tools/capture_vdp_presentation.c').read_text()
needle = 'retro_unload_game();retro_deinit();free(rom);return 0;'
capture = replace_once(capture, needle, '''const uint8_t*vr=dlsym(core,"vram");
 char dumpPath[4096];if(!vr||snprintf(dumpPath,sizeof dumpPath,"%s.vram",output)>=(int)sizeof dumpPath)return 2;
 FILE*dump=fopen(dumpPath,"wb");if(!dump)return 2;
 for(unsigned n=0;n<65536;n++)if(fputc(vr[n^1],dump)==EOF)return 2;
 fclose(dump);
 '''+needle)
(OUT / 'capture-combined.c').write_text(capture)
subprocess.run(['clang', '-O2', '-Wall', '-Wextra', '-Werror', '-DWILLOW_PRESENTATION_PHASES',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I', str(ROOT / 'tools'),
                str(OUT / 'capture-combined.c'), '-o', str(OUT / 'capture-combined')], check=True)
log = subprocess.check_output([str(OUT / 'capture-combined'), str(CORE), str(OUT / 'probe.bin'),
                               str(OUT / 'combined.ppm')], text=True)
(OUT / 'capture-combined.log').write_text(log)
assert (OUT / 'combined.ppm.vram').read_bytes() == (FIXTURE / 'vram.bin').read_bytes()
assert (OUT / 'combined.ppm').read_bytes() == (OUT / 'hardware.ppm').read_bytes()
assert log == (OUT / 'capture.log').read_text(), 'Read-only whole-VRAM observation changed native execution'
path = OUT / 'evidence.json'
evidence = json.loads(path.read_text())
evidence['scope'] = ('Static source-verified frame1200, prepared sprites plus native Window cache in one '
                     'carrier with a common immutable resource generation. Cold native preparation precedes '
                     'one measured steady publication. No live source producers, game logic, audio, cache '
                     'misses, VBlank admission or full-game FPS are measured.')
evidence['native_vram_bytes_exact'] = 65536
evidence['native_window_words_prepared'] = 1120
evidence['window_and_object_sat_erased_before_native_execution'] = True
evidence['second_latch_epoch'] = 2
evidence['cold_preparation_in_timed_phases'] = False
evidence['timing']['object_build_and_sat']['includes'] = 'native source-origin refresh, prepared object staging and unchanged Window readiness/publication'
evidence['steady_non_assertion_ms'] = sum(v['ms'] for k, v in evidence['timing'].items() if k != 'diagnostic_assertions')
for file in [Path(__file__).resolve(), ROOT / 'tools/native_prepared_scene.h',
             ROOT / 'src/willow_native_sat_publish.h',
             ROOT / 'src/willow_native_window_surface.h', ROOT / 'src/willow_native_presentation_plan.h',
             OUT / 'combined_fixture.h', OUT / 'inputs/combined-vram.bin',
             OUT / 'capture-combined.c', OUT / 'capture-combined.log', OUT / 'combined.ppm.vram']:
    evidence['hashes'][str(file)] = sha(file)
path.write_text(json.dumps(evidence, indent=2)+'\n')
print(json.dumps({'pixels': evidence['pixels'], 'vram_bytes': 65536,
                  'steady_non_assertion_ms': evidence['steady_non_assertion_ms'],
                  'timing': evidence['timing']}, indent=2))
