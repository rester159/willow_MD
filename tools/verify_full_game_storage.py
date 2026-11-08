#!/usr/bin/env python3
"""Validate the storage-only pack on host and headless 68000; never launch a GUI."""
from pathlib import Path
import argparse,hashlib,json,re,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main(folder):
    folder=folder.resolve();manifest=json.loads((folder/'manifest.json').read_text())
    for name,digest in manifest['outputs'].items():assert sha(folder/name)==digest
    attempt=1
    while (folder/f'native-check-v{attempt}').exists():attempt+=1
    out=folder/f'native-check-v{attempt}';out.mkdir(exist_ok=False);commands=[]
    def run(args):
        args=list(map(str,args));commands.append(args)
        p=subprocess.run(args,capture_output=True,text=True)
        (out/'last-stderr.txt').write_text(p.stderr)
        if p.returncode:print(p.stderr,file=sys.stderr);p.check_returncode()
        return p.stdout
    src=ROOT/'native/engine/tools'
    tinf=src.parent/'vendor/tinf/tinflate.c'
    assert manifest['codec']=='deflate'
    block_flag=f'-DWILLOW_STORAGE_BLOCK_BYTES={manifest["maximum_decoder_output_bytes"]}'
    run(['clang',block_flag,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',src/'check_storage_lzss.c',tinf,'-o',out/'host-check'])
    host=json.loads(run([out/'host-check',folder/'content.pack',folder/'expected-decoded.bin']))
    pack=(folder/'content.pack').read_bytes();assert 0x310000+len(pack)<=0x500000
    image=bytearray(b'\xff'*0x500000);image[0x200000:0x300000]=(folder/'pcm.bin').read_bytes()
    image[0x310000:0x310000+len(pack)]=pack
    (out/'storage_assets.h').write_text(f'#define STORAGE_PACK 0x310000UL\n#define STORAGE_BLOCKS {manifest["blocks"]}\n#define STORAGE_SENTINEL 0x{image[0x380000:0x380002].hex()}\n')
    (out/'assert.h').write_text('void willowStorageAssert(void);\n#define assert(x) ((x)?(void)0:willowStorageAssert())\n')
    run(['m68k-elf-gcc','-m68000','-Os','-Wall','-Wextra','-Werror','-Wno-sign-compare','-ffreestanding','-fno-builtin','-fstack-usage','-I',out,'-c',tinf,'-o',out/'tinf.o'])
    run(['m68k-elf-gcc',block_flag,'-m68000','-Os','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-fstack-usage','-nostdlib','-I',out,'-Wl,-Ttext=0x2000,-e,start',src/'storage_capacity_cartridge.c',out/'tinf.o','-lgcc','-o',out/'cartridge.elf'])
    run(['m68k-elf-objcopy','-O','binary',out/'cartridge.elf',out/'payload.bin'])
    symbols=run(['m68k-elf-nm','-n',out/'cartridge.elf']);entry=int(re.search(r'^(\w+) T start$',symbols,re.M)[1],16)
    payload=(out/'payload.bin').read_bytes();assert 0x2000+len(payload)<=0x100000
    image[0x2000:0x2000+len(payload)]=payload;struct.pack_into('>II',image,0,0xfffff0,entry)
    image[0x100:0x110]=b'SEGA SSF        ';image[0x1f0:0x200]=b'U               '
    (out/'storage-diagnostic.bin').write_bytes(image)
    run([sys.executable,ROOT/'tools/finalize_genesis_rom.py',out/'storage-diagnostic.bin'])
    run([sys.executable,ROOT/'tools/native_rom_audit.py',out/'storage-diagnostic.bin','--archive',ROOT/'willow.zip'])
    core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    assert sha(core)=='88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09'
    run(['clang','-DWILLOW_TEST_ROM_BYTES=5242880','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',src/'check_storage_capacity_native.c','-o',out/'native-check'])
    native=json.loads(run([out/'native-check',core,out/'storage-diagnostic.bin',folder/'expected-decoded.bin']))
    report=dict(scope=__doc__,host=host,native=native,diagnostic_code_bytes=len(payload),
        full_game=False,capacity_reservations_are_not_finished_code_or_tables=True,commands=commands,
        inputs={str(p):sha(p) for p in [Path(__file__).resolve(),folder/'manifest.json',src/'check_storage_lzss.c',src/'storage_capacity_cartridge.c',src/'check_storage_capacity_native.c',src.parent/'src/storage_decode.h',tinf,tinf.parent/'tinf.h',tinf.parent/'provenance.json',core]},
        outputs={p.name:sha(p) for p in out.iterdir() if p.is_file()})
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(host=host,native=native)))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=Path);main(p.parse_args().folder)
