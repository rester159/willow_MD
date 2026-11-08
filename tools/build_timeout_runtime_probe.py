#!/usr/bin/env python3
"""Build the headless 68000 timeout/scheduler diagnostic (does not launch an emulator)."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = root / 'build'
build.mkdir(exist_ok=True)
sgdk = Path(os.environ.get('SGDK', '/tmp/willow-sgdk-resume'))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir', type=Path, default=build, help='Isolated output directory for probe artifacts (default build/)')
parser.add_argument('--initial', action='store_true', help='Build main-task initial fade/scheduler diagnostic')
parser.add_argument('--reset', action='store_true', help='Include complete main reset entry after initial fade')
parser.add_argument('--scene', action='store_true', help='Continue reset into first scene setup')
parser.add_argument('--forest', action='store_true', help='Include complete forest-intro service bindings')
parser.add_argument('--native-record-words', action='store_true', help='Use alignment-safe 68000 record access for performance validation')
parser.add_argument('--no-partial-inlining', action='store_true', help='Measure whole-function specialization without GCC partial inlining')
parser.add_argument('--inline-actor-update', action='store_true', help='Keep actor-update bindings visible for target specialization')
parser.add_argument('--specialize-callbacks', action='store_true', help='Enable GCC constant-argument cloning for callback specialization')
parser.add_argument('--palette-add', action='store_true', help='Use native 16-word palette addition in the contiguous palette binding')
parser.add_argument('--inline-actor6-zero', action='store_true', help='Preserve alignment specialization in the frequent class-six selector-zero handler')
parser.add_argument('--inline-actor14-scan', action='store_true', help='Specialize fixed callbacks in the resumable class-fourteen slot scan')
parser.add_argument('--fast-rom-read', action='store_true', help='Check the immutable ROM range before mutable bus mappings')
parser.add_argument('--split-sprite-loops', action='store_true', help='Specialize sprite piece loops by the source frame format')
parser.add_argument('--direct-actor6-zero', action='store_true', help='Call the existing selector-zero handler directly from its pool scan')
parser.add_argument('--native-visibility', action='store_true', help='Execute the verified original visibility leaf as native 68000 code (forest source-RAM binding only)')
parser.add_argument('--split-stage-records', action='store_true', help='Keep stage-object empty-slot scans separate from record validation/dispatch')
parser.add_argument('--baseline', action='store_true', help='Disable the verified forest-probe defaults; explicit experimental flags still apply')
parser.add_argument('--optimize', choices=('O2', 'O3', 'Os'), default='O2', help='Target compiler optimization experiment (default O2)')
args = parser.parse_args()
output = args.output_dir
if not output.is_absolute():
    output = root / output
output.mkdir(parents=True, exist_ok=True)
native_sources = []
if args.native_visibility:
    if not args.forest:
        parser.error('--native-visibility requires --forest')
    native_assembly = output / 'native_visibility.S'
    subprocess.run([sys.executable, str(root / 'tools/generate_native_visibility.py'),
                    '--source', str(build / 'reference_maincpu.bin'),
                    '--output', str(native_assembly)], cwd=root, check=True)
    native_sources.append(str(native_assembly))
if args.forest and not args.baseline:
    # Full campaign parity verified for specialization, record access,
    # palette addition, actor-six/fourteen inlining, ROM-range-first reads
    # frame-format-specialized sprite loops, direct selector-zero calls
    # and compact stage-record scans.
    args.inline_actor_update = True
    args.specialize_callbacks = True
    args.native_record_words = True
    args.palette_add = True
    args.inline_actor6_zero = True
    args.inline_actor14_scan = True
    args.fast_rom_read = True
    args.split_sprite_loops = True
    args.direct_actor6_zero = True
    args.split_stage_records = True
if args.forest: args.scene = True
if args.scene: args.reset = True
stem = 'forest_runtime_probe' if args.forest else 'scene_runtime_probe' if args.scene else 'reset_runtime_probe' if args.reset else 'initial_runtime_probe' if args.initial else 'timeout_runtime_probe'
subprocess.run([
    'm68k-elf-gcc', '-m68000', '-' + args.optimize, '-Wall', '-Wextra', '-Werror',
    *(['-DWILLOW_NATIVE_RECORD_WORDS'] if args.native_record_words else []),
    *(['-fno-partial-inlining'] if args.no_partial_inlining else []),
    *(['-DWILLOW_INLINE_ACTOR_UPDATE'] if args.inline_actor_update else []),
    *(['-fipa-cp-clone'] if args.specialize_callbacks else []),
    *(['-DWILLOW_NATIVE_PALETTE_ADD'] if args.palette_add else []),
    *(['-DWILLOW_INLINE_ACTOR6_ZERO'] if args.inline_actor6_zero else []),
    *(['-DWILLOW_INLINE_ACTOR14_SCAN'] if args.inline_actor14_scan else []),
    *(['-DWILLOW_FAST_ROM_READ'] if args.fast_rom_read else []),
    *(['-DWILLOW_SPLIT_SPRITE_LOOPS'] if args.split_sprite_loops else []),
    *(['-DWILLOW_DIRECT_ACTOR6_ZERO'] if args.direct_actor6_zero else []),
    *(['-DWILLOW_NATIVE_VISIBILITY'] if args.native_visibility else []),
    *(['-DWILLOW_SPLIT_STAGE_RECORDS'] if args.split_stage_records else []),
    '-fno-builtin', '-ffreestanding', '-DSGDK_GCC', '-I' + str(sgdk / 'inc'),
    *(['-DWILLOW_TEST_INITIAL_SCHEDULER'] if args.initial or args.reset else []),
    *(['-DWILLOW_TEST_RESET_SCHEDULER'] if args.reset else []),
    *(['-DWILLOW_TEST_SCENE_SCHEDULER'] if args.scene else []),
    *(['-DWILLOW_TEST_FOREST_SCHEDULER'] if args.forest else []),
    '-I.', '-nostdlib', '-Wl,-T,tools/timeout_runtime_probe.ld',
    'tools/timeout_runtime_probe.c', *native_sources, '-lgcc', '-o', str(output / f'{stem}.elf'),
], cwd=root, check=True)
subprocess.run([
    'm68k-elf-objcopy', '-O', 'binary', '--only-section=.text',
    str(output / f'{stem}.elf'), str(output / f'{stem}.bin'),
], cwd=root, check=True)
with (output / f'{stem}.sym').open('w') as symbols:
    subprocess.run(['m68k-elf-nm', '-n', str(output / f'{stem}.elf')],
                   cwd=root, stdout=symbols, check=True)
print(f'Built {stem}; no emulator launched.')
