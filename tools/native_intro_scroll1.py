#!/usr/bin/env python3
"""Extract intro Scroll1 patterns directly from ROM task records and graphics.

This asset bank preserves the original fifteen opaque pen indices and both
8x8 layouts. No screenshots, recorded frames or fitted colors are inputs.
Palette fitting and display composition belong to the native renderer.
"""
from pathlib import Path
import json
import zipfile
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_text_task import decode
from cps1_palette import palette_words


def source_codes(program):
    codes = {0x7020, 0x71a6}
    groups = {0, 31}
    for message, style in [(46, 0x1302), (47, 0x1303), (49, 0x1305),
                           (0x419, 0x0306), (0x640, 0x1412), (0x641, 0x1413)]:
        task = decode(program, message, style)
        groups.update(a & 31 for a in task['attributes'])
        stream = task['stream']
        if task['handler'] == 6:
            for i in range(0, len(stream), 2):
                value = int.from_bytes(stream[i:i + 2], 'big')
                if value not in (0x26, 0x2f):
                    codes.update(0x7000 + value + n for n in (0, 1, 16, 17))
        else:
            base = 0x7100 if task['handler'] == 4 else 0x7000
            codes.update(base + value for value in stream if value not in (0x26, 0x2f))
    word = lambda a: int.from_bytes(program[a:a + 2], 'big')
    for phase in range(5):
        start = 0x7c7e + word(0x7c7e + phase * 2)
        pairs = 1 + program[0x7c78 + phase]
        for address in range(start, start + pairs * 8, 2):
            codes.update(0x7000 + word(address) + n for n in (0, 1, 16, 17))
    return sorted(codes), sorted(groups)


def main():
    program = assemble_maincpu(Path('willow.zip'))
    with zipfile.ZipFile('willow.zip') as archive:
        gfx = assemble_region(archive)
    codes, groups = source_codes(program)
    tiles = [bytes(32)]
    ids = {tiles[0]: 0}
    records = []
    for code in codes:
        record = [code]
        for layout in range(2):
            pens = decode_tile(gfx, code, 8, layout)
            # CPS pen 15 -> Genesis transparent 0; all opaque pens retain
            # their identity as 1..15, without color quantization or merging.
            tile = genesis_tiles([[(pen + 1) & 15 for pen in row] for row in pens])
            if tile not in ids:
                ids[tile] = len(tiles)
                tiles.append(tile)
            record.append(ids[tile])
        records.append(record)
    palettes = [[w | 0xf000 for w in palette_words(program, 'scroll1', 0, g)] for g in groups]
    Path('native/intro_probe/res/scroll1.bin').write_bytes(b''.join(tiles))
    rows = lambda values: ','.join('{' + ','.join(map(str, v)) + '}' for v in values)
    Path('native/intro_probe/src/scroll1.h').write_text(
        '/* Source pen indices; palette conversion is a separate renderer step. */\n'
        f'#define INTRO_SCROLL1_CODES {len(records)}\n'
        f'#define INTRO_SCROLL1_PATTERNS {len(tiles)}\n'
        'static const unsigned short intro_scroll1_codes[][3]={' + rows(records) + '};\n'
        'static const unsigned short intro_scroll1_groups[]={' + ','.join(map(str, groups)) + '};\n'
        'static const unsigned short intro_scroll1_cps_palettes[][16]={' + rows(palettes) + '};\n')
    report = {'codes': records, 'groups': groups, 'cps_palettes': palettes,
              'patterns': len(tiles), 'bytes': len(tiles) * 32,
              'source': 'Original program task records, graphics and Scroll1 palette ROM',
              'scope': 'Asset bank for pending native panel/title display integration'}
    Path('build/intro-scroll1-assets.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Scroll1: {len(codes)} codes, {len(tiles)} unique patterns, {len(groups)} original palette groups')


if __name__ == '__main__':
    main()
