#!/usr/bin/env python3
"""Build isolated all-video bridge from a hash-checked relocation manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/native-video-relocation'
SOURCE_SHA='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--program',type=Path,default=OUT/'program-addresses-v2.bin')
p.add_argument('--manifest',type=Path,default=OUT/'manifest-v2.json')
a=p.parse_args()
source=(ROOT/'build/reference_maincpu.bin').read_bytes()
assert hashlib.sha256(source).hexdigest()==SOURCE_SHA
manifest=json.loads(a.manifest.read_text());program=a.program.read_bytes()
assert manifest['source_sha256']==SOURCE_SHA and manifest['output_sha256']==hashlib.sha256(program).hexdigest()
expected=bytearray(source)
for patch in manifest['patches']:
 at,size=patch['offset'],patch['size']
 assert int.from_bytes(source[at:at+size],'big')==patch['source']
 expected[at:at+size]=patch['target'].to_bytes(size,'big')
assert bytes(expected)==program
OUT.mkdir(exist_ok=True)
(OUT/'bridge-program-input.bin').write_bytes(program)
(OUT/'bridge-program-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
subprocess.run(['m68k-elf-gcc','-m68000','-O2','-Wall','-Wextra','-Werror','-DSGDK_GCC','-I/tmp/willow-sgdk-resume/inc','-ffreestanding','-nostdlib','-Wl,-T,tools/native_video_bridge.ld','tools/native_frame_bridge_probe.c','tools/native_frame_bridge.S','-o',str(OUT/'bridge.elf')],cwd=ROOT,check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary','--only-section=.text',str(OUT/'bridge.elf'),str(OUT/'bridge.bin')],check=True)
with (OUT/'bridge.sym').open('w') as file:subprocess.run(['m68k-elf-nm','-n',str(OUT/'bridge.elf')],stdout=file,check=True)
print(f"Built simultaneous-video bridge from {len(manifest['patches'])} verified relocations")
