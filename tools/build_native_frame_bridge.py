#!/usr/bin/env python3
"""Build only the isolated native frame-context bridge, without shared outputs."""
import hashlib
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/native-frame-bridge'
out.mkdir(parents=True,exist_ok=True)
source=(root/'build/reference_maincpu.bin').read_bytes()
assert len(source)==0x100000 and hashlib.sha256(source).hexdigest()=='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
subprocess.run(['m68k-elf-gcc','-m68000','-O2','-Wall','-Wextra','-Werror','-DSGDK_GCC','-I/tmp/willow-sgdk-resume/inc','-ffreestanding','-nostdlib','-Wl,-T,tools/native_frame_bridge.ld','tools/native_frame_bridge_probe.c','tools/native_frame_bridge.S','-o',str(out/'bridge.elf')],cwd=root,check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary','--only-section=.text',str(out/'bridge.elf'),str(out/'bridge.bin')],check=True)
with (out/'bridge.sym').open('w') as file:
 subprocess.run(['m68k-elf-nm','-n',str(out/'bridge.elf')],stdout=file,check=True)
print(out/'bridge.bin')
