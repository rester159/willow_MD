#!/usr/bin/env python3
"""Closed host/actual68000 far-cache lifecycle proof, not live renderer admission."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import struct
import zipfile
from cps1_gfx import assemble_region, decode_tile

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_pixels(out):
    """Decode actual native resolved words/VRAM against original source pens."""
    frozen = ROOT/'build/native-route-background-audit/checkpoint-v1'
    fitted = json.loads((frozen/'prepared-v2/entries.json').read_text())
    entries = sorted((e for e in fitted if e['layer'] == 3), key=lambda e:(e['code'],e['attribute']))
    capture = (out/'events.csv.resolved.bin').read_bytes()
    assert len(capture) == len(entries)*16*34
    with zipfile.ZipFile(ROOT/'willow.zip') as archive:
        gfx = assemble_region(archive)
    pixels = 0
    for key, entry in enumerate(entries):
        code, attr = entry['code'], entry['attribute']
        masked = code & 0x3fff
        original = decode_tile(gfx, masked, 32) if 0xa00 <= masked <= 0xdff else [[15]*32 for _ in range(32)]
        for y in range(32):
            for x in range(32):
                sub = (y//8)*4+x//8
                at = (key*16+sub)*34
                name = struct.unpack_from('>H', capture, at)[0]
                raw = capture[at+2:at+34]
                nx, ny = x%8, y%8
                if name&0x800: nx = 7-nx
                if name&0x1000: ny = 7-ny
                pen = (raw[ny*4+nx//2] >> (0 if nx&1 else 4)) & 15
                source = original[31-y if attr&64 else y][31-x if attr&32 else x]
                assert pen == entry['pen_maps'][sub][source], (code,attr,x,y)
                assert (pen == 0) == (source == 15)
                if pen:
                    assert (name>>13)&3 == entry['references'][sub]>>14
                pixels += 1
    return {'source_keys':len(entries), 'actual_native_vram_pixels':pixels,
            'both_0c39_flip_keys': [attr for attr in (6,38) if any(e['code']==0xc39 and e['attribute']==attr for e in entries)],
            'scope':'Original source pens, prepared numeric pen fitting, actual native resolved words/VRAM; no captured positions, live plane or VBlank claim.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'build/native-far-cache/lifecycle-v1')
    parser.add_argument('--resource', type=Path, default=ROOT/'build/native-background-far-stream/prepared-v1')
    parser.add_argument('--catalog', type=Path, default=ROOT/'build/native-background-catalog/forest-277-v1')
    parser.add_argument('--residency', type=Path, default=ROOT/'build/native-background-residency/forest-277-v1')
    args = parser.parse_args()
    out = args.output.resolve()
    assert not out.exists(), 'Preserve previous evidence; choose a fresh output'
    out.mkdir(parents=True)
    resource, catalog, residency = (v.resolve() for v in (args.resource, args.catalog, args.residency))
    core = Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    inputs = [ROOT/name for name in ['src/willow_native_far_cache.h',
        'tools/native_far_cache_api.c', 'tools/test_native_far_cache.c',
        'tools/test_native_render_cache_closed.c', 'tools/vendor/libretro.h']]
    inputs += [Path(__file__).resolve(), core]
    inputs += [ROOT/'willow.zip', ROOT/'tools/cps1_gfx.py', ROOT/'build/native-route-background-audit/checkpoint-v1/prepared-v2/entries.json']
    for directory in (resource, catalog, residency):
        manifest = json.loads((directory/'manifest.json').read_text())
        for name, digest in manifest['outputs'].items():
            assert sha(directory/name) == digest, directory/name
            inputs.append(directory/name)
        inputs.append(directory/'manifest.json')
    before = {str(path): sha(path) for path in inputs}
    commands = []

    def run(command, log=None):
        commands.append(command)
        if log:
            with (out/log).open('w') as output:
                subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True)
        else:
            subprocess.run(command, cwd=ROOT, check=True)

    for path in inputs[:5]:
        (out/path.name).write_bytes(path.read_bytes())
    run(['python3', '-B', str(ROOT/'tools/verify_native_background_far_stream.py'),
         '--resource', str(resource), '--catalog', str(catalog), '--residency', str(residency),
         '--output', str(out/'resource-proof')], 'resource-proof.log')
    run(['m68k-elf-gcc', '-m68000', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
         '-ffreestanding', '-fno-builtin', '-nostdlib', '-I'+str(resource),
         '-Wl,-Ttext=0x102000,-e,willowFarCacheApiProbe',
         str(ROOT/'tools/native_far_cache_api.c'), '-lgcc', '-o', str(out/'api.elf')])
    run(['m68k-elf-objcopy', '-O', 'binary', str(out/'api.elf'), str(out/'api.bin')])
    symbols = subprocess.check_output(['m68k-elf-nm', '-n', str(out/'api.elf')], text=True)
    (out/'api.sym').write_text(symbols)
    entry = int(re.search(r'^([0-9a-f]+) T willowFarCacheApiProbe$', symbols, re.M)[1], 16)
    (out/'far-cache-fixture.h').write_text(f'#define API_ENTRY {entry:#x}\n')
    with (out/'api.disassembly').open('w') as output:
        subprocess.run(['m68k-elf-objdump', '-d', str(out/'api.elf')], stdout=output, check=True)
    run(['clang', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
         '-Wno-unused-function', '-Wno-unused-variable', '-fsanitize=address,undefined',
         '-fno-sanitize-recover=all', '-I'+str(out), '-I'+str(resource),
         str(ROOT/'tools/test_native_far_cache.c'), '-o', str(out/'test')])
    run([str(out/'test'), str(core), str(out/'api.bin'), str(resource/'far-stream.bin'),
         str(catalog/'catalog.bin'), str(residency/'physical-tiles.bin'), str(out/'events.csv')], 'test.log')
    result = json.loads((out/'test.log').read_text())
    pixels = source_pixels(out)
    assert result['physical_pool_boundaries'] == 165 and result['duplicate_requests'] > 0
    assert before == {str(path): sha(path) for path in inputs}, 'Input changed during proof'
    evidence = {'scope': __doc__, 'status': 'pass', 'result': result, 'source_pixel_proof':pixels,
        'checks': ['Exact prepared WBF1/WBC1 resources and initial physical image',
                   'All3968 far references and all248 complete source keys, including both0C39 flips',
                   'Queued old owner is unavailable before and after eviction, with stale/absent hints',
                   'All165 physical pool slots; duplicate requests share one slot and upload',
                   'Failed callbacks after each0..16word prefix never certify a partial owner',
                   'Later callback failure preserves earlier committed uploads; full retry is checked',
                   'During every native callback, COPYING rejects resolution, preparation binding and new reservation',
                   'Literal0A0C/0018 mask1111 pattern1430 consumes one epoch atMAX-2; further eviction refuses unchanged',
                   'Full protected pool refuses without state or VRAM changes',
                   'Two non-adjacent full planes union all165 slots in both orders, with attributes, duplicates and trailing zero runs',
                   'Cross-plane protected slots cannot be evicted: each sole free slot selected; full union refuses unchanged',
                   'Missing cells in either plane prevent binding; null/empty spans refuse without mutation',
                   'Two-plane partial publication retains both old uses; complete drainage retires them',
                   'Fresh source/palette/generation/view and residency binding',
                   'Partial publication preserves displayed/prepared union; complete drainage retires old uses',
                   'Every operation checks exact436native state bytes, all64KiB SRAM/VRAM, WRAM canaries, CRAM/VSRAM/registers and68000 C ABI'],
        'limitations': ['Closed API calls with explicit host seeding; no source route or live cache integration',
                        'Native callback writes physical VDP, but fixture display is disabled; no VBlank/deadline or FPS admission',
                        'Caller must resolve every retained canonical reference after residency changes and serialize preparation/upload/publication',
                        'Prepared catalog/palette subset only; no full-game asset coverage claim'],
        'inputs': before, 'commands': commands,
        'outputs': {str(path.relative_to(out)): sha(path) for path in out.rglob('*') if path.is_file()}}
    (out/'evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
