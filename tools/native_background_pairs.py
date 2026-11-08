#!/usr/bin/env python3
"""Convert all Stage 0 source layout pairs into native 8x8 tile references.

Lookup keys are the exact runtime Scroll2/Scroll3 dictionary IDs, not screen
positions or captured frames. Flip bits are baked into subtiles in map order.
The existing intro patterns/palettes are preserved. Extended IDs reserve the
existing forest, dialogue and mutable HUD namespaces.
"""
from pathlib import Path
from functools import lru_cache
import json
import re
import zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_palette import palette_words, cps_rgb
from native_palette_fit import color_error

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native/intro_probe'

def numbers(text):
    return [int(x, 0) for x in re.findall(r'0x[0-9a-f]+|\d+', text)]

def generate():
    program = assemble_maincpu(ROOT / 'willow.zip')
    with zipfile.ZipFile(ROOT / 'willow.zip') as archive:
        gfx = assemble_region(archive)
    header = (NATIVE / 'src/layers.h').read_text()
    words = numbers(header.split('intro_palettes[2][16]={')[1].split('};')[0])
    palettes = [[tuple(((w >> s) & 7) * 34 for s in (1, 5, 9))
                 for w in words[b*16:(b+1)*16]] for b in range(2)]
    original = (NATIVE / 'res/layers.bin').read_bytes()
    tiles = [original[i:i+32] for i in range(0, len(original), 32)]
    base = len(tiles)
    def define(path, name):
        return int(re.search(r'^#define '+name+r' (\d+)', path.read_text(), re.M).group(1))
    reserved = define(NATIVE/'src/forest.h', 'INTRO_FOREST_TILE_COUNT') + define(NATIVE/'src/overlay.h', 'INTRO_OVERLAY_TILES') + 200
    extension_base = base + reserved
    ids = {tile: i for i, tile in enumerate(tiles)}
    lines = ['/* Stage 0 ROM pair graphics. FFFF means unsupported pair. */',
             '#ifndef WILLOW_NATIVE_BACKGROUND_PAIRS_H',
             '#define WILLOW_NATIVE_BACKGROUND_PAIRS_H']
    report = {'stage': 0, 'base_patterns': base, 'layers': {},
              'scope': 'Pair-addressed graphics; horizontal streaming integration pending', 'reserved_pattern_ids': reserved, 'extension_base': extension_base,
              'palette_source': 'Original CPS RGB888 fitted to existing two intro palettes'}

    @lru_cache(None)
    def nearest(color):
        return tuple(min((color_error(color, rgb), i)
                         for i, rgb in enumerate(palette) if i)
                     for palette in palettes)

    @lru_cache(None)
    def pixels(size, code):
        return decode_tile(gfx, code, size)

    for layer, start, end, stride, pointers, modes in [
            (2, 0x80000, 0xc0000, 0x400, 0x2dbf8, 0x2dc1c),
            (3, 0xc0000, 0xd0000, 0x100, 0x2eb24, 0x2eb44)]:
        def record(at):
            code = int.from_bytes(program[at:at+2], 'big')
            attr = int.from_bytes(program[at+2:at+4], 'big')
            if layer == 3:
                code = (code + 0xa00) & 65535
            return (code << 16) | attr
        records = [record(at) for at in range(start, end, 4)]
        pairs = sorted(set(records) | {0} | ({int.from_bytes(program[0x8404:0x8408],'big')} if layer == 3 else set()))
        pointer = int.from_bytes(program[pointers:pointers+4], 'big')
        mode = program[modes]
        # Complete index domain of the original chunk selector, including
        # unused/repeated layout slots; no guessed playable-width cutoff.
        domain = (256, 256, 2048, 512)[mode]
        chunks = sorted(set(program[pointer:pointer+domain]))
        supported = {record(start + chunk*stride + offset)
                     for chunk in chunks for offset in range(0, stride, 4)}
        supported.add(0x0a000000 if layer == 3 else 0)
        size = 16 if layer == 2 else 32
        n = size // 8
        source_palettes = [[cps_rgb(w | 0xf000) for w in
                            palette_words(program, f'scroll{layer}', 0, group)]
                           for group in range(32)]
        offsets, entries = [], []
        for pair in pairs:
            if pair not in supported:
                offsets.append(65535)
                continue
            offsets.append(len(entries))
            code, attr = pair >> 16, pair & 65535
            code = 0x4000 + (code & 0x1fff) if layer == 2 else code
            image = pixels(size, code) if layer == 2 or 0xa00 <= code <= 0xdff else [[15]*size for _ in range(size)]
            for ty in range(n):
                for tx in range(n):
                    colors = []
                    for y in range(ty*8, ty*8+8):
                        for x in range(tx*8, tx*8+8):
                            pen = image[size-1-y if attr & 64 else y][size-1-x if attr & 32 else x]
                            colors.append(None if pen == 15 else source_palettes[attr & 31][pen])
                    bank = min(range(2), key=lambda b: sum(nearest(c)[b][0] for c in colors if c is not None))
                    pens = [0 if c is None else nearest(c)[bank][1] for c in colors]
                    tile = genesis_tiles([pens[y:y+8] for y in range(0, 64, 8)])
                    if tile not in ids:
                        ids[tile] = len(tiles)
                        tiles.append(tile)
                    pattern = ids[tile]
                    entries.append((pattern if pattern < base else pattern + reserved) | bank << 15)
        assert len(entries) < 65535 and len(tiles) + reserved < 32768
        for name, values in [('Offsets', offsets), ('Entries', entries)]:
            lines.append(f'static const unsigned short nativeBackground{layer}{name}[]={{')
            lines.extend(','.join(map(str, values[i:i+16]))+',' for i in range(0, len(values), 16))
            lines.append('};')
        report['layers'][str(layer)] = {'layout': pointer, 'mode': mode, 'chunks': chunks,
                                       'dictionary_pairs': len(pairs), 'supported_pairs': len(supported),
                                       'subtiles': len(entries)}
    lines.extend([f'#define NATIVE_BACKGROUND_BASE_PATTERNS {base}',
                  f'#define NATIVE_BACKGROUND_PATTERN_COUNT {len(tiles)}',
                  f'#define NATIVE_BACKGROUND_EXTENSION_BASE {extension_base}',
                  f'#define NATIVE_BACKGROUND_ID_LIMIT {len(tiles)+reserved}', '#endif'])
    (NATIVE / 'src/background_pairs.h').write_text('\n'.join(lines)+'\n')
    (NATIVE / 'res/background_pairs.bin').write_bytes(b''.join(tiles[base:]))
    report.update(patterns=len(tiles), new_patterns=len(tiles)-base,
                  new_pattern_bytes=(len(tiles)-base)*32)
    (ROOT / 'build/native-background-pairs.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'layers'}))

if __name__ == '__main__':
    generate()
