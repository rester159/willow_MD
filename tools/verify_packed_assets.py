#!/usr/bin/env python3
"""Check every legal object/background tile through the banked native loader.
This exercises resource lookup/cache/VDP publication, not gameplay or FPS.
"""
from pathlib import Path
import argparse,hashlib,json,re,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main(folder,out,components=None):
    folder=folder.resolve();out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((folder/'manifest.json').read_text())
    for name,digest in manifest['outputs'].items():assert sha(folder/name)==digest
    assert manifest['codec']=='deflate' and manifest['maximum_decoder_output_bytes']==1024
    assert [s['name'] for s in manifest['streams']]==['all_physical_gfx_native16','scroll1_left','scroll1_right','scroll3','all_original_adpcm']
    commands=[]
    def run(args):
        args=list(map(str,args));commands.append(args)
        p=subprocess.run(args,capture_output=True,text=True);(out/'last-stderr.txt').write_text(p.stderr)
        if p.returncode:print(p.stderr,file=sys.stderr);p.check_returncode()
        return p.stdout
    src=ROOT/'native/engine/tools';tinf=src.parent/'vendor/tinf/tinflate.c'
    run(['clang','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined',src/'check_packed_assets.c',tinf,'-o',out/'host-check'])
    host=json.loads(run([out/'host-check',folder/'content.pack',folder/'expected-decoded.bin']))
    pack=(folder/'content.pack').read_bytes();assert 0x310000+len(pack)<=0x500000
    image=bytearray(b'\xff'*0x500000);image[0x200000:0x300000]=(folder/'pcm.bin').read_bytes();image[0x310000:0x310000+len(pack)]=pack
    extra=[];checks=[]
    if components:
        for item in components:
            path=Path(item['path']).resolve();data=path.read_bytes();at=item['offset']
            if sha(path)!=item['sha256']:raise ValueError(f'Changed component: {path}')
            if not data or at<0x10000 or at+len(data)>0x200000 or at&1:
                raise ValueError(f'Invalid component extent: {path}')
            if any(at<r['end'] and r['offset']<at+len(data) for r in extra):
                raise ValueError(f'Overlapping component: {path}')
            image[at:at+len(data)]=data
            checksum=2166136261
            for b in data:checksum=((checksum^b)*16777619)&0xffffffff
            extra.append(dict(item,path=str(path),bytes=len(data),end=at+len(data),fnv32=checksum))
            checks.append(f'{{0x{at:x}UL,{len(data)}UL,0x{checksum:08x}UL}}')
    (out/'storage_assets.h').write_text(f'#define STORAGE_PACK 0x310000UL\n#define STORAGE_PACK_BYTES {len(pack)}UL\n#define STORAGE_SENTINEL 0x{image[0x380000:0x380002].hex()}\n'+
        (f'#define STORAGE_COMPONENT_COUNT {len(extra)}\nstatic const uint32_t storageComponents[][3]={{'+','.join(checks)+'};\n' if extra else ''))
    (out/'assert.h').write_text('void willowStorageAssert(void);\n#define assert(x) ((x)?(void)0:willowStorageAssert())\n')
    run(['m68k-elf-gcc','-m68000','-Os','-Wall','-Wextra','-Werror','-Wno-sign-compare','-ffreestanding','-fno-builtin','-fstack-usage','-I',out,'-c',tinf,'-o',out/'tinf.o'])
    run(['m68k-elf-gcc','-m68000','-Os','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-fstack-usage','-nostdlib','-I',out,'-Wl,-Ttext=0x2000,-e,start',src/'packed_assets_cartridge.c',out/'tinf.o','-lgcc','-o',out/'cartridge.elf'])
    run(['m68k-elf-objcopy','-O','binary',out/'cartridge.elf',out/'payload.bin'])
    symbols=run(['m68k-elf-nm','-n',out/'cartridge.elf']);entry=int(re.search(r'^(\w+) T start$',symbols,re.M)[1],16)
    payload=(out/'payload.bin').read_bytes();assert 0x2000+len(payload)<=0x10000
    workspace_at=int(re.search(r'^(\w+) [Rr] packedAssetWorkspaceBytes$',symbols,re.M)[1],16)-0x2000
    workspace=struct.unpack_from('>I',payload,workspace_at)[0]
    image[0x2000:0x2000+len(payload)]=payload;struct.pack_into('>II',image,0,0xfffff0,entry)
    image[0x100:0x110]=b'SEGA SSF        ';image[0x1f0:0x200]=b'U               '
    rom=out/'asset-loader-diagnostic.bin';rom.write_bytes(image)
    run([sys.executable,ROOT/'tools/finalize_genesis_rom.py',rom]);run([sys.executable,ROOT/'tools/native_rom_audit.py',rom,'--archive',ROOT/'willow.zip'])
    core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    assert sha(core)=='88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09'
    run(['clang','-DWILLOW_TEST_ROM_BYTES=5242880','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',src/'check_packed_assets_native.c','-o',out/'native-check'])
    native=json.loads(run([out/'native-check',core,rom,folder/'expected-decoded.bin']))
    inputs=[Path(__file__).resolve(),folder/'manifest.json',src/'check_packed_assets.c',src/'packed_assets_cartridge.c',src/'check_packed_assets_native.c',src.parent/'src/packed_assets.h',src.parent/'src/packed_assets_rom.h',src.parent/'src/storage_decode.h',src.parent/'src/rom_window.h',src.parent/'src/sprite_span_vdp.h',tinf,tinf.parent/'tinf.h',core]+[Path(r['path']) for r in extra]
    report=dict(scope=__doc__,host=host,native=native,code_bytes=len(payload),persistent_asset_cache_bytes=workspace,
        full_game=False,live_memory_layout_proven=False,components=extra,commands=commands,inputs={str(p):sha(p) for p in inputs},outputs={p.name:sha(p) for p in out.iterdir() if p.is_file()})
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['host','native','code_bytes','persistent_asset_cache_bytes']}))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();main(a.folder,a.output)
