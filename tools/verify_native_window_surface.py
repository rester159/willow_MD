#!/usr/bin/env python3
"""Verify prepared native Window words and dirty rows, with headless VDP pixels."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
from PIL import Image
from build_vdp_presentation_probe import ROOT, build

OUT = ROOT / 'build/native-window-surface'
FIXTURE = ROOT / 'build/cartridge-gap-audit/frame1200_window_v1'
CORE = Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run():
    OUT.mkdir(exist_ok=True)
    source = ROOT / 'tools/test_native_window_surface.c'
    subprocess.run(['clang', '-Wall', '-Wextra', '-Werror', '-std=c99', '-O2',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(ROOT / 'src'), str(source), '-o', str(OUT / 'host-test')], check=True)
    host_log = subprocess.check_output([str(OUT / 'host-test')], text=True)
    (OUT / 'host.log').write_text(host_log)
    manifest = json.loads((FIXTURE / 'manifest.json').read_text())
    assert manifest['source_composition_pixels_verified'] == 71680
    assert manifest['source_composition_mismatches'] == 0
    assert manifest['window'] == [0xd000, 0xdfff]
    for filename, info in manifest['buffers'].items():
        assert (FIXTURE / filename).stat().st_size == info['bytes']
        assert sha(FIXTURE / filename) == info['sha256']
    vram = bytearray((FIXTURE / 'vram.bin').read_bytes())
    words = struct.unpack('>2048H', vram[0xd000:0xe000])
    window = [words[y*64+x] for y in range(28) for x in range(40)]
    assert all(word == 0 for i, word in enumerate(words) if i//64 >= 28 or i%64 >= 40)
    colors = struct.unpack('>64H', (FIXTURE / 'cram.bin').read_bytes())
    header = '/* Pre-resolved native words from the verified source frame. */\n'
    for name, values in [('fixtureWindow', window), ('fixtureColors', colors)]:
        header += f'static const uint16_t {name}[]={{\n'
        header += '\n'.join(','.join(str(v) for v in values[n:n+16])+',' for n in range(0, len(values), 16))
        header += '\n};\n'
    (OUT / 'window_surface_fixture.h').write_text(header)
    inputs = OUT / 'inputs'
    inputs.mkdir(exist_ok=True)
    vram[0xd000:0xe000] = bytes(4096)
    (inputs / 'vram.bin').write_bytes(vram)
    for name in ['cram', 'vsram', 'registers']:
        (inputs / f'{name}.bin').write_bytes((FIXTURE / f'{name}.bin').read_bytes())
    build({name: inputs / f'{name}.bin' for name in ['vram', 'cram', 'vsram', 'registers']},
          OUT, native_source=ROOT / 'tools/native_window_surface_presentation.c')
    capture = (ROOT / 'tools/capture_vdp_presentation.c').read_text()
    old = '"text_budget","raw_capture","object_build_and_sat","diagnostic_assertions","static_vdp_upload"'
    new = '"scene_cache_init","initial_cell_preparation","full_window_upload","unchanged_publish","one_row_edit_and_upload"'
    assert capture.count(old) == 1
    capture = capture.replace(old, new)
    needle = 'retro_unload_game();retro_deinit();free(rom);return 0;'
    assert capture.count(needle) == 1
    # Read the real VDP memory after native stores. Preserve all off-screen
    # cells and other allocations, not just the pixels visible in the crop.
    capture = capture.replace(needle, '''const uint8_t*vr=dlsym(core,"vram");
 char dumpPath[4096];if(!vr||snprintf(dumpPath,sizeof dumpPath,"%s.vram",output)>=(int)sizeof dumpPath)return 2;
 FILE*dump=fopen(dumpPath,"wb");if(!dump)return 2;
 for(unsigned n=0;n<65536;n++)if(fputc(vr[n^1],dump)==EOF)return 2;
 fclose(dump);
 '''+needle)
    (OUT / 'capture.c').write_text(capture)
    subprocess.run(['clang', '-Wall', '-Wextra', '-Werror', '-O2',
                    '-DWILLOW_PRESENTATION_PHASES', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(ROOT / 'tools'),
                    str(OUT / 'capture.c'), '-o', str(OUT / 'capture')], check=True)
    log = subprocess.check_output([str(OUT / 'capture'), str(CORE), str(OUT / 'probe.bin'),
                                   str(OUT / 'hardware.ppm')], text=True)
    (OUT / 'capture.log').write_text(log)
    actual_vram = (OUT / 'hardware.ppm.vram').read_bytes()
    expected_vram = (FIXTURE / 'vram.bin').read_bytes()
    assert actual_vram == expected_vram, 'Native Window publication changed VRAM or padding unexpectedly'
    timing = {}
    for line in log.splitlines():
        if line.startswith('PHASE '):
            _, name, cycles, ms = line.split()
            timing[name] = {'master_cycles': int(cycles), 'milliseconds': float(ms)}
    expected_image = Image.open(FIXTURE / manifest['expected_image']).convert('RGB')
    expected = [((r>>3)*255//31, (g>>2)*255//63, (b>>3)*255//31)
                for r, g, b in expected_image.get_flattened_data()]
    actual = Image.open(OUT / 'hardware.ppm').convert('RGB')
    assert actual.size == (320, 224)
    bad = sum(a != b for a, b in zip(actual.get_flattened_data(), expected))
    actual.save(OUT / 'hardware.png')
    files = [Path(__file__).resolve(), source, ROOT / 'src/willow_native_window_surface.h',
             ROOT / 'src/willow_native_presentation_plan.h', ROOT / 'tools/native_window_surface_presentation.c',
             ROOT / 'tools/capture_vdp_presentation.c', ROOT / 'tools/build_vdp_presentation_probe.py',
             FIXTURE / 'manifest.json', CORE] + [OUT / name for name in
             ['window_surface_fixture.h', 'capture.c', 'capture.log', 'host.log', 'probe.bin', 'probe.sym', 'native.su', 'hardware.png', 'hardware.ppm.vram']]
    report = {'scope': 'Prepared native Window words and dirty-row publisher; captured frame1200 only. '
              'Source-write hooks, resource ownership, live VBlank deadlines and full-game FPS are not established.',
              'host': json.loads(host_log), 'native_pixels': 71680, 'mismatched_pixels': bad,
              'native_vram_bytes_exact': len(actual_vram),
              'timing': timing, 'timing_scope': 'Actual68000 with display disabled. Initial cells are compiled Genesis words. '
              'Includes epoch/readiness checks; excludes other renderer work, source logic, cache misses, active-display contention and audio.',
              'stack_usage': (OUT / 'native.su').read_text(),
              'sha256': {str(p): sha(p) for p in files}}
    (OUT / 'evidence.json').write_text(json.dumps(report, indent=2)+'\n')
    assert bad == 0, report
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    run()
