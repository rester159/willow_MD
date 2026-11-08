#!/usr/bin/env python3
"""Check pair assets through the independent source-coordinate renderer."""
from pathlib import Path
from functools import lru_cache
import re
import json
import zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region
from cps1_palette import palette_words, cps_rgb
from cps1_source_layers import SourceLayers
from native_palette_fit import color_error

root = Path(__file__).resolve().parents[1]
native = root / 'native/intro_probe'

def array(text, name):
    return [int(v, 0) for v in re.findall(r'0x[0-9a-f]+|\d+', text.split(name+'[]={')[1].split('};')[0])]

program = assemble_maincpu(root/'willow.zip')
with zipfile.ZipFile(root/'willow.zip') as archive:
    gfx = assemble_region(archive)
source = SourceLayers(program, gfx)
header = (native/'src/background_pairs.h').read_text()
intro = (native/'src/layers.h').read_text()
words = [int(v, 0) for v in re.findall(r'0x[0-9a-f]+|\d+', intro.split('intro_palettes[2][16]={')[1].split('};')[0])]
palettes = [[tuple(((w >> s) & 7)*34 for s in (1,5,9)) for w in words[b*16:(b+1)*16]] for b in range(2)]
base=int(re.search(r'#define NATIVE_BACKGROUND_BASE_PATTERNS (\d+)',header).group(1))
extension=int(re.search(r'#define NATIVE_BACKGROUND_EXTENSION_BASE (\d+)',header).group(1))
blob = (native/'res/layers.bin').read_bytes() + bytes((extension-base)*32) + (native/'res/background_pairs.bin').read_bytes()
assert len(blob) % 32 == 0
maps = array(intro, 'intro_maps')
lookup = {}
entries_by_layer = {}
checked = pixels = initial = 0

@lru_cache(None)
def nearest(c, bank):
    return min((color_error(c, rgb), i) for i,rgb in enumerate(palettes[bank]) if i)

for layer in (2,3):
    pairs = array((native/f'src/scroll{layer}_data.h').read_text(), f'nativeScroll{layer}Pairs')
    offsets = array(header, f'nativeBackground{layer}Offsets')
    entries = array(header, f'nativeBackground{layer}Entries')
    assert len(offsets) == len(pairs)
    lookup[layer] = {pair:i for i,pair in enumerate(pairs)}
    entries_by_layer[layer] = (offsets, entries)
    palette = [[cps_rgb(w|0xf000) for w in palette_words(program,f'scroll{layer}',0,g)] for g in range(32)]
    pointer, mode = source.maps[layer]
    # Enumerate coordinates in the source selector domain, then check each
    # distinct source pair once, independent of generator chunk enumeration.
    xbits = (4,5,6,8)[mode]
    domain = (256,256,2048,512)[mode]
    size = 16 if layer == 2 else 32
    n = size//8
    seen = set()
    chunks = set()
    for index in range(domain):
        chunk = program[pointer+index]
        if chunk in chunks:
            continue
        chunks.add(chunk)
        ox = (index & ((1<<xbits)-1))*256
        oy = (index >> xbits)*256
        for y in range(0,256,size):
            for x in range(0,256,size):
                code,attr = source.record(layer,ox+x,oy+y)
                address = (0x80000 if layer==2 else 0xc0000) + chunk*(0x400 if layer==2 else 0x100) + ((x//size)*(256//size)+y//size)*4
                raw = int.from_bytes(program[address:address+2], 'big')
                pair = ((raw if layer==2 else (raw+0xa00)&65535)<<16)|attr
                if pair in seen:
                    continue
                seen.add(pair)
                off = offsets[lookup[layer][pair]]
                assert off != 65535, (layer, hex(pair))
                for sy in range(n):
                    for sx in range(n):
                        entry = entries[off+sy*n+sx]
                        assert (entry&32767)<base or (entry&32767)>=extension
                        tile = blob[(entry&32767)*32:(entry&32767)*32+32]
                        assert len(tile)==32
                        actual = [pen for b in tile for pen in (b>>4,b&15)]
                        colors = []
                        for py in range(8):
                            for px in range(8):
                                pen,group = source.pen(layer,ox+x+sx*8+px,oy+y+sy*8+py)
                                colors.append(None if pen==15 else palette[group][pen])
                        bank = min(range(2),key=lambda b:sum(nearest(c,b)[0] for c in colors if c is not None))
                        expected = [0 if c is None else nearest(c,bank)[1] for c in colors]
                        assert entry>>15==bank and actual==expected, (layer,hex(pair),sx,sy)
                        checked += 1
                        pixels += 64
    assert all(offset==65535 for pair,offset in zip(pairs,offsets)
               if pair not in seen and pair != (0 if layer==2 else 0xa000000))

# Every original intro cell must retain precisely its tile ID and palette.
for layer,origin,height,base in [(2,-752,736,0),(3,-496,480,3680)]:
    size=16 if layer==2 else 32
    offsets,entries=entries_by_layer[layer]
    for ty in range(height//8):
        for tx in range(40):
            x,y=32+tx*8,origin+ty*8
            code,attr=source.record(layer,x,y)
            # SourceLayers masks Scroll2's graphics code, whereas live map IDs
            # preserve the original word (including blank FFFF records).
            candidates=[p for p in lookup[layer] if (p&65535)==attr and
                        (0x4000+((p>>16)&0x1fff) if layer==2 else p>>16)==code]
            pair=next(p for p in candidates if offsets[lookup[layer][p]]!=65535)
            off=offsets[lookup[layer][pair]]
            assert off != 65535
            entry=entries[off+(y%size//8)*(size//8)+x%size//8]
            assert entry==maps[base+ty*40+tx], (layer,tx,ty,entry,maps[base+ty*40+tx])
            initial+=1
report={'source_subtiles_checked':checked,'source_pixels_checked':pixels,'unchanged_intro_cells':initial,
        'scope':'ROM asset parity; no native streaming or performance claim'}
(root/'build/native-background-pairs-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
