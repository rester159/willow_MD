#!/usr/bin/env python3
"""Build guarded optional palette leaf hooks without changing source artifacts."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE_SHA='0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1'
SITES=[
    (0x95e,0x976,'paletteEpochRaw','3e3c001f22d822d822d822d822d822d822d822d851cfffee'),
    (0x9fc,0xa10,'paletteEpochCopy','22d822d822d822d822d822d822d822d851c9ffee'),
    (0xc92,0xcd0,'paletteEpochAnimated','7001221824110282f000f000828222c1221824110282f000f000828222c1221824110282f000f000828222c1221824110282f000f000828222c151c8ffc6'),
    (0x526e,0x527e,'paletteEpochFade','3c3c0003d558d558d558d55851cefff6'),
]

def sha(data):return hashlib.sha256(data).hexdigest()

def build(out,address):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'build/reference_maincpu.bin').read_bytes()
    assert sha(source)==SOURCE_SHA
    linker=out/'hooks.ld'
    linker.write_text(f'SECTIONS {{ . = {address:#x}; .text : {{ *(.text*) }} /DISCARD/ : {{ *(.comment*) *(.note*) }} }}\n')
    elf=out/'hooks.elf'
    subprocess.run(['m68k-elf-gcc','-m68000','-ffreestanding','-nostdlib',f'-Wl,-T,{linker}',
                    str(ROOT/'tools/native_palette_epoch_hooks.S'),'-o',str(elf)],check=True)
    subprocess.run(['m68k-elf-objcopy','-O','binary',str(elf),str(out/'hooks.bin')],check=True)
    symbols_text=subprocess.check_output(['m68k-elf-nm','-n',str(elf)],text=True)
    (out/'hooks.sym').write_text(symbols_text)
    (out/'hooks.disasm').write_text(subprocess.check_output(['m68k-elf-objdump','-d',str(elf)],text=True))
    symbols={r.split()[2]:int(r.split()[0],16) for r in symbols_text.splitlines() if len(r.split())==3}
    blob=(out/'hooks.bin').read_bytes();patches=[]
    assert address+len(blob)<=0x110000
    assert symbols['paletteEpochHooksStart']==address and symbols['paletteEpochHooksEnd']==address+len(blob)
    # These four bodies use caller-supplied original/native destination pointers;
    # no bus-specific operands occur, so both exact variants are byte-identical.
    (out/'hooks-original.bin').write_bytes(blob)
    header=['/* Generated closed-test addresses; not a production header. */']
    for at,end,name,expected_hex in SITES:
        expected=bytes.fromhex(expected_hex)
        assert source[at:end]==expected,(hex(at),source[at:end].hex(),expected_hex)
        assert source[end:end+2]==bytes.fromhex('4e75')
        # Compare every relocated source instruction byte including backward
        # branch displacements; epilogue deliberately replaces only final RTS.
        body=symbols[name+'Body']-address;done=symbols[name+'End']-address
        assert blob[body:done]==expected,(name,blob[body:done].hex(),expected_hex)
        patches.append({'address':at,'expected':source[at:at+6].hex(),
                        'replacement':'4ef9'+symbols[name].to_bytes(4,'big').hex(),
                        'target':name,'original_body_start':at,'original_rts':end,
                        'original_body_sha256':sha(expected)})
        for label in (name,name+'Body',name+'End'):
            header.append(f'#define {label.upper()} {symbols[label]:#x}u')
    (out/'hooks-test.h').write_text('\n'.join(header)+'\n')
    manifest={'schema_version':1,'scope':'Optional exact original palette leaf bodies plus native epoch invalidation; requires closed and route admission.',
              'source_rom_sha256':SOURCE_SHA,'rom_start':address,'bytes':len(blob),'blob_sha256':sha(blob),
              'symbols':symbols,'source_patches':patches,
              'metadata':{'epoch':0x20f568,'ready_word':0x20f56c,'busy_word':0x20f56e},
              'contract':'BUSY increments before READY clears and epoch increments; BUSY decrements after all original stores. No-write5260 paths remain unchanged. Certify only BUSY0/epoch-nonzero, with epoch and busy rechecks before publishing READY. Native hooks require initialized metadata and nesting depth<65535. Reset clears READY/BUSY and starts a fresh nonzero lease domain. Epoch overflow must refuse or reinitialize all dependent leases.',
              'sources':{n:sha((ROOT/n).read_bytes()) for n in ['tools/native_palette_epoch_hooks.S','tools/build_native_palette_epoch_hooks.py']}}
    base=ROOT/'build/native-object-publication/checkpoint-compact-v1'
    pinned=json.loads((base/'checkpoint.json').read_text())['files']
    products={};inputs={}
    for suffix in ('','-original'):
        for kind in ('program','platform'):
            name=kind+suffix+'.bin';data=(base/name).read_bytes()
            assert sha(data)==pinned[name],name
            inputs[name]=sha(data)
            if kind=='program': result=apply_hooks(data,manifest)
            else:
                offset=address-0x100010
                assert len(data)<=offset
                result=data+bytes(offset-len(data))+blob
            (out/name).write_bytes(result);products[name]=sha(result)
    manifest['base_inputs']=inputs;manifest['products']=products
    manifest['bus_variants']='Original/native use identical indirect-pointer hook bytes. Both guarded program variants and appended platform variants are emitted from immutable compact-v1; no shared artifact is changed.'
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def apply_hooks(program,manifest):
    program=bytearray(program)
    for patch in manifest['source_patches']:
        at=patch['address'];old=bytes.fromhex(patch['expected']);new=bytes.fromhex(patch['replacement'])
        assert program[at:at+len(old)]==old,hex(at)
        program[at:at+len(old)]=new
    return bytes(program)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/native-palette-epoch-audit/hooks-v1')
    parser.add_argument('--address',type=lambda x:int(x,0),default=0x10e000)
    args=parser.parse_args();m=build(args.output,args.address)
    print(json.dumps({'bytes':m['bytes'],'symbols':m['symbols']},indent=2))


if __name__=='__main__':main()
