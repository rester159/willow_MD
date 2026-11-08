#!/usr/bin/env python3
"""Build the dedicated original-startup/full-IRQ diagnostic inputs."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/native-video-relocation'
subprocess.run(['python3', 'tools/build_native_io_test.py'], cwd=ROOT, check=True)
subprocess.run(['m68k-elf-gcc', '-m68000', '-ffreestanding', '-nostdlib',
                '-Wl,-Ttext=0x100010,-e,nativeSourceIRQProbe',
                'tools/native_source_irq_probe.S', '-o', str(OUT/'source-irq-probe.elf')],
               cwd=ROOT, check=True)
subprocess.run(['m68k-elf-objcopy', '-O', 'binary', '--only-section=.text',
                str(OUT/'source-irq-probe.elf'), str(OUT/'source-irq-probe.bin')], check=True)
symbols = subprocess.check_output(['m68k-elf-nm', '-n', str(OUT/'source-irq-probe.elf')], text=True)
(OUT/'source-irq-probe.sym').write_text(symbols)
parsed = {row.split()[2]:int(row.split()[0],16) for row in symbols.splitlines() if len(row.split())==3}
assert parsed['nativeSourceIRQProbe']==0x100010
assert parsed['nativeSourceIRQReturn']==0x100012
assert parsed['nativeSourceIRQCarrier']==0x100016
assert (OUT/'source-irq-probe.bin').stat().st_size==18
print('Native TRAP carrier addresses and size verified; source1554/RTE remains unchanged')
