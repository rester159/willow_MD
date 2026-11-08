#!/usr/bin/env python3
"""Build the headless 68000 gameplay diagnostic (does not launch an emulator)."""
from pathlib import Path
import argparse
import os
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / 'build'
build.mkdir(exist_ok=True)
sgdk = Path(os.environ.get('SGDK', '/tmp/willow-sgdk-resume'))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--routed', action='store_true', help='Include outer gameplay routing and timeout services')
parser.add_argument('--main', action='store_true', help='Include main task and manager around actual gameplay services')
args = parser.parse_args()
stem = 'gameplay_main_probe' if args.main else 'gameplay_routed_probe' if args.routed else 'gameplay_runtime_probe'
subprocess.run([
    'm68k-elf-gcc', '-m68000', '-O2', '-Wall', '-Wextra', '-Werror',
    '-fno-builtin', '-ffreestanding', '-DSGDK_GCC', '-I' + str(sgdk / 'inc'),
    *(['-DWILLOW_TEST_GAMEPLAY_ROUTED'] if args.routed else []),
    *(['-DWILLOW_TEST_GAMEPLAY_MAIN'] if args.main else []),
    '-I.', '-nostdlib', '-Wl,-T,tools/gameplay_runtime_probe.ld',
    'tools/gameplay_runtime_probe.c', '-lgcc', '-o', f'build/{stem}.elf',
], cwd=root, check=True)
subprocess.run([
    'm68k-elf-objcopy', '-O', 'binary', '--only-section=.text',
    f'build/{stem}.elf', f'build/{stem}.bin',
], cwd=root, check=True)
with (build / f'{stem}.sym').open('w') as symbols:
    subprocess.run(['m68k-elf-nm', '-n', f'build/{stem}.elf'],
                   cwd=root, stdout=symbols, check=True)
print(f'Built {stem}; no emulator launched.')
