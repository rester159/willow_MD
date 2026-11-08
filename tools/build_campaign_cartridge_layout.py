#!/usr/bin/env python3
"""Place measured campaign components in a headless 5 MiB storage checkpoint.

This deliberately does not produce a release: translated campaign code still
has diagnostic bindings, and several final runtime resources are unresolved.
No allowance, source executable, or estimated compression ratio is packed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

import verify_packed_assets

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 5 * 1024 * 1024


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--storage', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    parser.add_argument('--spawns', type=Path, required=True)
    parser.add_argument('--stage-palettes', type=Path, required=True)
    parser.add_argument('--background-start', type=Path, required=True)
    parser.add_argument('--player-start', type=Path, required=True)
    parser.add_argument('--logic', type=Path, required=True)
    parser.add_argument('--player-animation', type=Path)
    parser.add_argument('--player-templates', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    manifests = {}
    components = []
    cursor = 0x10000

    def resource(folder, name, role, offset=None, bound=False):
        nonlocal cursor
        folder = folder.resolve()
        manifest = folder / 'manifest.json'
        metadata = json.loads(manifest.read_text())
        path = folder / name
        if metadata['outputs'].get(name) != digest(path):
            raise ValueError(f'Component differs from its build manifest: {path}')
        manifests[str(manifest)] = digest(manifest)
        at = (cursor + 1) & ~1 if offset is None else offset
        item = dict(path=str(path), sha256=digest(path), offset=at, role=role,
                    final_runtime_bound=bound)
        components.append(item)
        if offset is None:
            cursor = at + path.stat().st_size
            if cursor > 0x100000:
                raise ValueError('Fixed tables overlap the translated code link address')

    for name, role in [
        ('world.bin', 'All six stages: scenery and collision cells'),
        ('contact.bin', 'Collision material rules'),
        ('player-probes.bin', 'Player collision probes'),
        ('camera-loads.bin', 'Authored camera loads'),
        ('camera-bounds.bin', 'Authored camera bounds'),
        ('palette-program.bin', 'Original-word animated palette programs'),
        ('stage-one-palettes.bin', 'Stage-one palette selectors'),
    ]:
        resource(args.world, name, role)
    resource(args.spawns, 'spawns.bin', 'All 13 authored spawn tables / 386 entries')
    resource(args.stage_palettes, 'stage-palettes.bin', 'All six stage palette initializers')
    resource(args.background_start, 'background-start.bin', 'All six background initializers')
    resource(args.player_start, 'player-start.bin', 'All 106 checkpoint records')
    if bool(args.player_animation) != bool(args.player_templates):
        raise ValueError('Player animation and canonical templates must be supplied together')
    if args.player_animation:
        animations = json.loads((args.player_animation / 'manifest.json').read_text())
        templates = json.loads((args.player_templates / 'manifest.json').read_text())
        if templates.get('art_binding') != 'canonical packed object codes':
            raise ValueError('Player templates require a separate unaccounted pattern bank')
        if [n['source'] for n in animations['nodes']] != templates['poses']:
            raise ValueError('Player animation and sprite pose IDs do not match')
        for name in sorted(animations['outputs']):
            resource(args.player_animation, name, 'Compiled player animation/HUD/action data: ' + name)
        resource(args.player_templates, 'templates.bin', 'Player templates referencing canonical packed object codes')
    # Retain the measured compiled code at its actual linked address. It is
    # inspected for storage only, never invoked with incompatible RAM bindings.
    logic_metadata = json.loads((args.logic / 'manifest.json').read_text())
    for name in ['forest_runtime_probe.sym', 'forest_runtime_probe.elf']:
        if digest(args.logic / name) != logic_metadata['outputs'][name]:
            raise ValueError('Translated campaign link evidence changed: ' + name)
    symbols = (args.logic / 'forest_runtime_probe.sym').read_text()
    first_text = re.search(r'^(\w+) [Tt] ', symbols, re.M)
    if first_text is None or int(first_text[1], 16) != 0x100010:
        raise ValueError('Unexpected translated campaign link address')
    resource(args.logic, 'forest_runtime_probe.bin',
             'Translated full campaign code; diagnostic bindings unresolved', 0x100010)
    # Source-equivalence checks operate on immutable source files on the host;
    # the arcade executable is not included in the cartridge.
    checks = {}
    commands = []

    def run(command):
        command = list(map(str, command))
        commands.append(command)
        return subprocess.check_output(command, cwd=ROOT, text=True)

    for name, source, data in [
        ('world', 'check_world.c', args.world / 'world.bin'),
        ('spawns', 'check_stage_spawns.c', args.spawns / 'spawns.bin'),
        ('checkpoints', 'check_player_start.c', args.player_start / 'player-start.bin'),
    ]:
        executable = out / ('check-' + name)
        run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
             '-Wno-unused-function',
             '-fsanitize=address,undefined', ROOT / 'native/engine/tools' / source,
             '-o', executable])
        checks[name] = json.loads(run([executable, ROOT / 'build/reference_maincpu.bin', data]))
    (out / 'components.json').write_text(json.dumps(components, indent=2) + '\n')
    verify_packed_assets.main(args.storage, out / 'native-check', components)
    native = json.loads((out / 'native-check/manifest.json').read_text())
    if native['native']['fixed_components_checked'] != len(components):
        raise ValueError('Not every fixed-ROM component was checked on the 68000')
    rom = out / 'native-check/asset-loader-diagnostic.bin'
    if rom.stat().st_size != LIMIT:
        raise ValueError('Actual cartridge byte-size check failed')
    segments = [dict(role='Vectors/header', offset=0, bytes=0x200),
                dict(role='Executed resource loader and validation code', offset=0x2000,
                     bytes=native['code_bytes'])]
    segments += [dict(role=r['role'], offset=r['offset'], bytes=r['bytes'],
                      path=r['path'], sha256=r['sha256'],
                      final_runtime_bound=r['final_runtime_bound']) for r in native['components']]
    for name, at, role in [('pcm.bin', 0x200000, 'Existing 16 kHz PCM bank'),
                           ('content.pack', 0x310000, 'All graphics and original ADPCM, losslessly packed')]:
        path = args.storage.resolve() / name
        segments.append(dict(role=role, offset=at, bytes=path.stat().st_size,
                             path=str(path), sha256=digest(path)))
    segments.sort(key=lambda r: r['offset'])
    free = []
    previous = 0
    for segment in segments:
        if segment['offset'] < previous:
            raise ValueError('Overlapping final image segments')
        if segment['offset'] > previous:
            free.append(dict(offset=previous, bytes=segment['offset'] - previous))
        previous = segment['offset'] + segment['bytes']
    if previous < LIMIT:
        free.append(dict(offset=previous, bytes=LIMIT - previous))
    unresolved = [
        'Final link joining native gameplay, renderer, input, audio and startup',
        'Campaign code replacement of source-data reads and diagnostic RAM/SRAM bindings',
        'Complete behavior/animation/sprite-layout/UI/shop/ending table catalog for that link',
        'Live palette fitting, sprite construction and VRAM residency using canonical packed art',
        'Native music/sequencer/driver code and tables, plus live sound playback integration',
        'Full campaign execution and visual checks of the actual linked cartridge',
    ]
    result = dict(scope=__doc__, cartridge_limit_bytes=LIMIT,
                  actual_image_bytes=rom.stat().st_size, actual_image_sha256=digest(rom),
                  measured_component_bytes=sum(s['bytes'] for s in segments),
                  unallocated_and_padding_bytes=sum(s['bytes'] for s in free),
                  segments=segments, unallocated_ranges=free,
                  placeholder_allowance_bytes=0, included_original_cpu_executable_bytes=0,
                  measured_components_fit=True, complete_game_capacity_proven=False,
                  performance_gate_open=False, release_eligible=False,
                  unresolved=unresolved, source_parity=checks, native_checks=native['native'],
                  input_manifests=manifests, commands=commands,
                  builder_sha256=digest(Path(__file__).resolve()))
    (out / 'layout.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ['actual_image_bytes', 'measured_component_bytes',
          'unallocated_and_padding_bytes', 'measured_components_fit',
          'complete_game_capacity_proven', 'release_eligible']}))


if __name__ == '__main__':
    main()
