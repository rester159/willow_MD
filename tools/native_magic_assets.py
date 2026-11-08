#!/usr/bin/env python3
"""Bake source Willow level-zero magic into his existing Genesis palette.
Runtime trace: class-8 records FF C786/C746, source EC84 and EFBC-EFC8.
No arcade program bytes are linked into the cartridge.
"""
from pathlib import Path
import zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_palette import sprite_pen_map, palette_words, cps_rgb, genesis_color
from cps1_animation_catalog import follow_sequence, be16

def main():
    p=assemble_maincpu(Path('willow.zip'))
    with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
    remap=sprite_pen_map(p,0,8,0)
    rows=['/* Generated from source graphics and animation metadata. */']
    words=[0]+[genesis_color(cps_rgb(w|0xf000)) for w in palette_words(p,'base',0,8)[:15]]
    rows.append('static const unsigned short native_magic_palette[16]={'+','.join(map(str,words))+'};')
    rows.append('static const u32 native_magic_tiles[2][4][4][128] = {')
    for mode in range(2):
        rows.append('{')
        for kind in range(4):
            records=follow_sequence(p,0xecfe+be16(p,0xecfe+kind*2),0)['records']
            assert len(records)==4
            rows.append('{')
            for record in records:
                words=[int(w,16) for w in record['words']]
                width=((words[1]>>8)&15)+1;height=((words[1]>>12)&15)+1
                assert words[4]==3 and width<=2 and height==2
                pixels=[[0]*32 for _ in range(32)]
                for ty in range(height):
                    for tx in range(width):
                        tile=decode_tile(g,words[5]+ty*16+tx,16)
                        for y in range(16):
                            for x in range(16):pixels[ty*16+y][tx*16+x]=(remap[(tile[y][x]+1)&15] if mode else (tile[y][x]+1)&15)
                data=b''.join(genesis_tiles([row[x:x+8] for row in pixels[y:y+8]])
                              for x in range(0,32,8) for y in range(0,32,8))
                rows.append('{'+','.join('0x'+data[i:i+4].hex() for i in range(0,len(data),4))+'},')
            rows.append('},')
        rows.append('},')
    rows.append('};')
    # At initial weapon level (a5+366=0), source CB08/CB4C maps charge to 0..3.
    levels=[p[0xcb4c+p[0xcb08+((n+1)>>3)]] for n in range(128)]
    assert max(levels)==3
    rows.append('static const unsigned char native_magic_charge_level[128]={'+','.join(map(str,levels))+'};')
    Path('build/native_magic_assets.h').write_text('\n'.join(rows)+'\n')

if __name__=='__main__':main()
