#!/usr/bin/env python3
"""Check an exported practice scenery crop on the headless Genesis VDP.

This validates packed artwork, palette pages and pixels only. It is not a
playable campaign or a cartridge-capacity/performance test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--stage', type=int, choices=(1,3,5), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assets, out = args.assets.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    header = (assets/'sorsha_assets.h').read_text()
    def values(name):
        body = re.search(r'\b'+name+r'\[[^=]+?=\s*\{(.*?)\};', header, re.S)[1]
        return [int(v,0) for v in re.findall(r'0x[0-9a-fA-F]+|\d+',body)]
    index = (1,3,5).index(args.stage)
    offset, count = values('sorsha_bg_offsets')[index], values('sorsha_bg_counts')[index]
    patterns = (assets/'sorsha_backgrounds.bin').read_bytes()[offset:offset+count*32]
    tilemap = values('sorsha_bg_maps')[index*1120:(index+1)*1120]
    colors = [0]*32+values('sorsha_bg_palettes')[index*32:(index+1)*32]
    assert len(patterns)==count*32 and count*32<=0xc000 and len(tilemap)==1120 and len(colors)==64
    assert all((v>>13&3) in (2,3) and (v&2047)<count and not(v&0x9800) for v in tilemap)
    plane=[0]*2048
    for y in range(28): plane[y*64:y*64+40]=tilemap[y*40:y*40+40]
    def array(name, data):
        return 'static const unsigned short '+name+'[]={'+','.join(hex(v) for v in data)+'};\n'
    (out/'assets.h').write_text(array('patterns',struct.unpack('>'+str(len(patterns)//2)+'H',patterns))+array('plane',plane)+array('colors',colors))
    (out/'display.c').write_text('''#include "assets.h"
static volatile unsigned short*const data=(void*)0xc00000;
static volatile unsigned short*const control=(void*)0xc00004;
static void address(unsigned short at){*(volatile unsigned long*)control=0x40000000UL|((unsigned long)(at&0x3fff)<<16)|(at>>14);}
__attribute__((noreturn)) void start(void){
 __asm__ volatile("move.w #0x2700,%sr");
 if(*(volatile unsigned char*)0xa10001&15)*(volatile unsigned long*)0xa14000=0x53454741;
 *control=0x8104;*control=0x8004;*control=0x8230;*control=0x8334;*control=0x8407;*control=0x8578;*control=0x8700;*control=0x8b00;*control=0x8c81;*control=0x8d3e;*control=0x8f02;*control=0x9001;*control=0x9100;*control=0x9200;
 address(0);for(unsigned n=0;n<32768;n++)*data=0;
 address(0);for(unsigned n=0;n<sizeof(patterns)/2;n++)*data=patterns[n];
 address(0xc000);for(unsigned n=0;n<2048;n++)*data=plane[n];
 *(volatile unsigned long*)control=0xc0000000;for(unsigned n=0;n<64;n++)*data=colors[n];
 *(volatile unsigned long*)control=0x40000010;*data=0;*data=0;
 *control=0x8144;*(volatile unsigned long*)0xff0000=0x56445031;
 for(;;)__asm__ volatile("nop");
}
''')
    commands=[]
    def run(command):
        command=list(map(str,command));commands.append(command)
        return subprocess.check_output(command,cwd=ROOT,text=True)
    run(['m68k-elf-gcc','-m68000','-O2','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-nostdlib','-Wl,-Ttext=0x200,-e,start',out/'display.c','-lgcc','-o',out/'display.elf'])
    run(['m68k-elf-objcopy','-O','binary',out/'display.elf',out/'payload.bin'])
    entry=int(re.search(r'^([0-9a-f]+) T start$',run(['m68k-elf-nm','-n',out/'display.elf']),re.M)[1],16)
    rom=bytearray(0x200)+(out/'payload.bin').read_bytes()
    struct.pack_into('>2I',rom,0,0xffff00,entry);rom[0x100:0x110]=b'SEGA GENESIS    ';rom[0x1f0:0x200]=b'U               '
    (out/'display.bin').write_bytes(rom)
    core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    assert sha(core)=='88314cd8554a3cc5eebf00a4139e6f0dee9ea579dcb2321a0959579f1902fa09'
    run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror',ROOT/'tools/capture_vdp_presentation.c','-o',out/'capture'])
    log=run([out/'capture',core,out/'display.bin',out/'actual.ppm']);(out/'capture.log').write_text(log)
    actual=(out/'actual.ppm').read_bytes().split(b'\n',3)[3];assert len(actual)==320*224*3
    format=int(re.search(r'format(\d)',log)[1]);expected=bytearray();preview=bytearray()
    for y in range(224):
        for x in range(320):
            word=tilemap[y//8*40+x//8];raw=patterns[(word&2047)*32+(y%8)*4+(x%8)//2]
            pen=(raw>>(0 if x&1 else 4))&15
            assert pen, 'Exported scenery must be opaque'
            cram=colors[(word>>13&3)*16+pen];rgb=[(cram>>s&7)*34 for s in (1,5,9)];preview.extend(rgb)
            if format!=1:rgb=[(rgb[0]>>3)*255//31,(rgb[1]>>(2 if format==2 else 3))*255//(63 if format==2 else 31),(rgb[2]>>3)*255//31]
            expected.extend(rgb)
    assert bytes(preview)==Image.open(assets/f'sorsha-stage-{args.stage}-preview.png').convert('RGB').tobytes()
    mismatch=sum(actual[i:i+3]!=expected[i:i+3] for i in range(0,len(actual),3))
    Image.frombytes('RGB',(320,224),actual).save(out/'actual.png')
    assert mismatch==0,mismatch
    paths=[Path(__file__),ROOT/'tools/sorsha_assets.py',ROOT/'tools/cps1_gameplay_maps.py',ROOT/'tools/cps1_stage_catalog.py',ROOT/'tools/cps1_gfx.py',ROOT/'tools/cps1_palette.py',ROOT/'tools/cps1_program.py',ROOT/'tools/native_palette_fit.py',ROOT/'tools/capture_vdp_presentation.c',ROOT/'willow.zip',ROOT/'assets/sorsha/sheet.png',ROOT/'assets/sorsha/layout.json',core,assets/'sorsha_assets.h',assets/'sorsha_backgrounds.bin',assets/f'sorsha-stage-{args.stage}-preview.png']
    result=dict(scope=__doc__,stage=args.stage,checked_pixels=320*224,mismatched_pixels=mismatch,background_palette_pages=[2,3],player_palette_pages_reserved=[0,1],resident_patterns=count,live_gameplay=False,matching_longplay_verified=False,inputs={str(p):sha(p) for p in paths},commands=commands,outputs={p.name:sha(p) for p in out.iterdir() if p.is_file()})
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('inputs','outputs','commands')},indent=2))

if __name__=='__main__':main()
