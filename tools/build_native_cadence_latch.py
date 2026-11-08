#!/usr/bin/env python3
"""Compose a seven-region, no-renderer native Genesis cadence experiment."""
import hashlib
import json
from pathlib import Path
import subprocess
from native_io_relocation import relocate

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/native-cadence/latch-v2'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'build/reference_maincpu.bin').read_bytes()
candidate,io_manifest=relocate(source,with_video=True)
candidate=bytearray(candidate)
assert source[0x6980:0x6986]==bytes.fromhex('207c00922000')
candidate[0x6982:0x6986]=(0x20c800).to_bytes(4,'big')
parsed=None
platform_hashes={}
for name,defines in [('platform',[]),('platform-original',['-DCADENCE_ORIGINAL_BUS=1'])]:
 subprocess.run(['m68k-elf-gcc','-m68000','-O2','-ffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror',*defines,
  '-Wl,-T,tools/native_cadence_latch.ld','tools/native_cadence_latch_platform.S','tools/native_cadence_latch.c','-o',str(OUT/(name+'.elf'))],cwd=ROOT,check=True)
 subprocess.run(['m68k-elf-objcopy','-O','binary',str(OUT/(name+'.elf')),str(OUT/(name+'.bin'))],check=True)
 symbols=subprocess.check_output(['m68k-elf-nm','-n',str(OUT/(name+'.elf'))],text=True)
 (OUT/(name+'.sym')).write_text(symbols)
 (OUT/(name+'.disasm')).write_text(subprocess.check_output(['m68k-elf-objdump','-d',str(OUT/(name+'.elf'))],text=True))
 current={row.split()[2]:int(row.split()[0],16) for row in symbols.splitlines() if len(row.split())==3}
 if parsed is not None:assert parsed==current, 'Original/native platform symbol layout differs'
 parsed=current
 platform_hashes[name]=hashlib.sha256((OUT/(name+'.bin')).read_bytes()).hexdigest()
assert parsed['nativeCadenceBoot']==0x100010
# Both native platforms differ only in three checked absolute addresses. Their
# instruction lengths and register allocation are identical.
native=(OUT/'platform.bin').read_bytes();original=(OUT/'platform-original.bin').read_bytes()
assert len(native)==len(original)
patches=[];rebuilt=bytearray(original)
for before,after in [(0x800100,0x20f900),(0x900000,0x20c000),(0x908000,0x20d000)]:
 needle=before.to_bytes(4,'big');positions=[n for n in range(0,len(original)-3,2) if original[n:n+4]==needle]
 assert len(positions)==1,(hex(before),positions)
 at=positions[0];assert native[at:at+4]==after.to_bytes(4,'big')
 rebuilt[at:at+4]=after.to_bytes(4,'big');patches.append({'address':0x100010+at,'source':before,'target':after,'width':4})
assert rebuilt==native,'Uncatalogued platform difference'
manifest={'scope':'seven-region native Genesis cadence experiment plus scattered raw object latch; SRAM20C800 holds row-scroll; no renderer or production map admission',
 'source_sha256':hashlib.sha256(source).hexdigest(),'program_sha256':hashlib.sha256(candidate).hexdigest(),
 'io_video':io_manifest,'rowscroll_patch':{'offset':0x6982,'source':0x922000,'target':0x20c800,'size':4,'source_instruction':'207c00922000'},
 'platform_symbols':{n:parsed[n] for n in ('nativeCadenceBoot','nativeCadenceVBlank','nativeCadenceLatch')},
 'platform_sha256':platform_hashes,'platform_address_patches':patches,
 'latch_header_sha256':hashlib.sha256((ROOT/'src/willow_native_object_latch.h').read_bytes()).hexdigest(),
 'latch_scope':'32x64-byte conditional WRAM chunks; pre-IRQ previous hardware port selects raw full2KiB bank',
 'ntsc_master_hz':53693175,'ntsc_lines':262,'master_cycles_per_line':3420,
 'reference_clock_scope':'Both paths execute on the same Genesis CPU/VDP clock; this is not CPS10MHz/59.6374Hz speed equivalence.'}
(OUT/'program.bin').write_bytes(candidate)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(OUT/'platform-addresses.h').write_text(f'#define CADENCE_BOOT {parsed["nativeCadenceBoot"]:#x}u\n#define CADENCE_IRQ {parsed["nativeCadenceVBlank"]:#x}u\n#define CADENCE_LATCH {parsed["nativeCadenceLatch"]:#x}u\n#define CADENCE_LATCH_END {parsed["willowNativeObjectLatchOffsets"]:#x}u\n#define CADENCE_IO_MIRROR {io_manifest["mirror"]:#x}u\n')
print(f'Composed separate native cadence program {manifest["program_sha256"]}; IRQ vector target {parsed["nativeCadenceVBlank"]:x}')
