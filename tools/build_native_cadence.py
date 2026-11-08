#!/usr/bin/env python3
"""Compose a seven-region, no-renderer native Genesis cadence experiment."""
import hashlib
import json
from pathlib import Path
import subprocess
from native_io_relocation import relocate

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/native-cadence'
OUT.mkdir(exist_ok=True)
source=(ROOT/'build/reference_maincpu.bin').read_bytes()
candidate,io_manifest=relocate(source,with_video=True)
candidate=bytearray(candidate)
assert source[0x6980:0x6986]==bytes.fromhex('207c00922000')
candidate[0x6982:0x6986]=(0x20c800).to_bytes(4,'big')
subprocess.run(['m68k-elf-gcc','-m68000','-ffreestanding','-nostdlib',
 '-Wl,-Ttext=0x100010,-e,nativeCadenceBoot','tools/native_cadence_platform.S','-o',str(OUT/'platform.elf')],cwd=ROOT,check=True)
subprocess.run(['m68k-elf-objcopy','-O','binary','--only-section=.text',str(OUT/'platform.elf'),str(OUT/'platform.bin')],check=True)
symbols=subprocess.check_output(['m68k-elf-nm','-n',str(OUT/'platform.elf')],text=True)
(OUT/'platform.sym').write_text(symbols)
parsed={row.split()[2]:int(row.split()[0],16) for row in symbols.splitlines() if len(row.split())==3}
assert parsed['nativeCadenceBoot']==0x100010
manifest={'scope':'seven-region native Genesis cadence experiment; row-scroll replaces displayed latch; no renderer or production map admission',
 'source_sha256':hashlib.sha256(source).hexdigest(),'program_sha256':hashlib.sha256(candidate).hexdigest(),
 'io_video':io_manifest,'rowscroll_patch':{'offset':0x6982,'source':0x922000,'target':0x20c800,'size':4,'source_instruction':'207c00922000'},
 'platform_symbols':{n:parsed[n] for n in ('nativeCadenceBoot','nativeCadenceVBlank')},
 'platform_sha256':hashlib.sha256((OUT/'platform.bin').read_bytes()).hexdigest(),
 'ntsc_master_hz':53693175,'ntsc_lines':262,'master_cycles_per_line':3420,
 'reference_clock_scope':'Both paths execute on the same Genesis CPU/VDP clock; this is not CPS10MHz/59.6374Hz speed equivalence.'}
(OUT/'program.bin').write_bytes(candidate)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(OUT/'platform-addresses.h').write_text(f'#define CADENCE_BOOT {parsed["nativeCadenceBoot"]:#x}u\n#define CADENCE_IRQ {parsed["nativeCadenceVBlank"]:#x}u\n#define CADENCE_IO_MIRROR {io_manifest["mirror"]:#x}u\n')
print(f'Composed separate native cadence program {manifest["program_sha256"]}; IRQ vector target {parsed["nativeCadenceVBlank"]:x}')
