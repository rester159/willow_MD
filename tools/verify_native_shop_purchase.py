#!/usr/bin/env python3
"""Headless original-68000 / native-C shop purchase proof; no GUI launch."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA = '0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/native-shop-purchase/verified-v1'))
    parser.add_argument('--core', type=Path, default=Path.home() / 'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
    parser.add_argument('--continuous-decline', action='store_true', help='Also replay existing controller-only decline regression through native/host/original main, task scan and wake')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'build/reference_maincpu.bin'
    if digest(source) != SOURCE_SHA:
        raise SystemExit('Unexpected original ROM hash')
    commands = []

    def run(command, name, extra_env=None):
        commands.append(command)
        with (out / name).open('w') as log:
            subprocess.run(command, cwd=ROOT, check=True, stdout=log, stderr=subprocess.STDOUT,
                           env={**os.environ, **(extra_env or {})})

    common = ['clang', '-Wall', '-Wextra', '-Werror', '-O2', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
    elf = out / 'native-probe.elf'
    run(['m68k-elf-gcc', '-m68000', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin',
         '-fstack-usage', '-ffunction-sections', '-fdata-sections', '-nostdlib',
         '-Wl,-Ttext=0x180000,-e,shopNativeProbe,--gc-sections', 'tools/native_shop_purchase_probe.c', '-lgcc', '-o', str(elf)], 'native-build.log')
    run(['m68k-elf-objcopy', '-O', 'binary', str(elf), str(out / 'native-probe.bin')], 'objcopy.log')
    run(['m68k-elf-nm', '-n', str(elf)], 'native-probe.sym')
    symbols = (out / 'native-probe.sym').read_text()
    entry = int(re.search(r'^([0-9a-fA-F]+) T shopNativeProbe$', symbols, re.M).group(1), 16)
    for name in ('controls', 'entry', 'purchase'):
        run(common + [f'tools/test_arcade_shop_{name}.c', '-o', str(out / f'{name}-test')], f'{name}-build.log')
        command = [str(out / f'{name}-test'), str(args.core)]
        if name == 'purchase':
            command += [str(out / 'native-probe.bin'), hex(entry)]
        run(command, f'{name}.log', {'WILLOW_SHOP_ORACLE': str(out / 'source-consumer-oracle.bin')})
    text = (out / 'purchase.log').read_text()
    required = ('33728 original68000 closed cases', '3036 ticks in46 episodes',
                '13 invalid/missing bindings', '1040 cases, exact non-stack RAM')
    if not all(item in text for item in required):
        raise SystemExit('Incomplete purchase proof output')
    if args.continuous_decline:
        route = out / 'decline-regression'
        route.mkdir(exist_ok=True)
        run([sys.executable, 'tools/build_timeout_runtime_probe.py', '--forest', '--output-dir', str(route)], 'decline-build.log')
        # Fixture logic is unchanged: only its hard-coded diagnostic artifact
        # paths are redirected so other campaign evidence is not overwritten.
        fixture = (ROOT / 'tools/test_native_forest_scheduler.c').read_text()
        fixture = fixture.replace('build/forest_runtime_probe.', str(route / 'forest_runtime_probe.'))
        fixture = fixture.replace('build/native-shop-entry-investigation/', str(route) + '/')
        fixture = fixture.replace('build/forest-', str(route / 'forest-'))
        (route / 'test-route.c').write_text(fixture)
        run(common + ['-Itools', '-DWILLOW_TEST_FIRST_LEVEL', '-DWILLOW_TEST_BOSS_INPUT',
                      '-DWILLOW_TEST_SHOP_ENTRY', '-DWILLOW_TEST_SHOP_EXIT', str(route / 'test-route.c'),
                      '-o', str(route / 'test-route')], 'decline-test-build.log')
        run([str(route / 'test-route'), str(args.core), '1'], 'decline.log', {'WILLOW_PLAY_TICKS': '4096'})
        if 'Shop decline and exit: 1 completed exits' not in (out / 'decline.log').read_text():
            raise SystemExit('Continuous decline route did not complete')
    # Record transitive local headers, not just the new wrapper. These are the
    # implementations actually compiled into both host and target proof.
    dependencies = {Path(__file__).resolve(), source}
    for name in ('tools/native_shop_purchase_probe.c', 'tools/test_arcade_shop_purchase.c',
                 'tools/test_arcade_shop_controls.c', 'tools/test_arcade_shop_entry.c'):
        dependency_text = subprocess.check_output(['clang', '-MM', name], cwd=ROOT, text=True).replace('\\\n', ' ')
        dependencies.update((ROOT / item).resolve() for item in dependency_text.split(':', 1)[1].split())
    if args.continuous_decline:
        dependencies.update((ROOT / 'tools' / name) for name in
                            ('build_timeout_runtime_probe.py', 'test_native_forest_scheduler.c',
                             'timeout_runtime_probe.c', 'timeout_runtime_probe.ld'))
    evidence = {
        'status': 'pass', 'source_rom_sha256': SOURCE_SHA,
        'core': str(args.core), 'core_sha256': digest(args.core), 'native_entry': entry,
        'native_code_bytes': (out / 'native-probe.bin').stat().st_size,
        'closed_cases': 33728, 'persistent_ticks': 3036, 'persistent_episodes': 46,
        'compiled_native_cases': 1040, 'negative_cases': 13,
        'continuous_decline': args.continuous_decline,
        'source_entries': ['33BD6', '33E7C', '33EF2', '33F98', '3400A', '34272', '3436E', '347B6'],
        'limitations': [
            'Diagnostic bindings only; no rendered Genesis shop or full-game claim.',
            'Persistent closed sequences supply asynchronous text completion equally to both consumers; the sequence does not run those text tasks.',
            'Native C ABI and live D4.w/A4 are checked; private source registers and final source CCR are not emulated by the C API.',
            'Paid dialogue selector 4 remains unsupported; tested source table selects 0 or 2.',
            'Optional continuous route is existing decline regression, not a continuous paid purchase.'
        ],
        'inputs': {str(path.relative_to(ROOT)): digest(path) for path in sorted(dependencies)},
        'outputs': {str(path.relative_to(out)): digest(path) for path in sorted(out.rglob('*')) if path.is_file() and path != out / 'evidence.json'},
        'commands': commands,
    }
    (out / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'PASS: {out / "evidence.json"}')


if __name__ == '__main__':
    main()
