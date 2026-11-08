#!/usr/bin/env python3
"""Flatten the stationary source forest to free a Genesis text plane.

Unchanged foreground tiles retain their existing encoding. Mixed tiles are
fitted from original CPS RGB, not from already-quantized Genesis colors.
"""
from pathlib import Path
import re
import json
import zipfile
from collections import Counter
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region, genesis_tiles
from cps1_source_layers import SourceLayers
from cps1_palette import palette_words, cps_rgb
from native_palette_fit import color_error

root = Path('native/intro_probe')
header = (root / 'src/layers.h').read_text()
numbers = lambda s: [int(v, 0) for v in re.findall(r'0x[0-9a-f]+|\d+', s)]
maps = numbers(header.split('intro_maps[]={')[1].split('};')[0])
words = numbers(header.split('intro_palettes[2][16]={')[1].split('};')[0])
palettes = [[tuple(((w >> shift) & 7) * 34 for shift in (1, 5, 9))
             for w in words[i * 16:(i + 1) * 16]] for i in range(2)]
blob = (root / 'res/layers.bin').read_bytes()
base = len(blob) // 32
tiles = [blob[i:i + 32] for i in range(0, len(blob), 32)]
ids = {t: i for i, t in enumerate(tiles)}
p = assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:
    gfx = assemble_region(z)
source = SourceLayers(p, gfx)
source_palettes = {layer: [[cps_rgb(w | 0xf000) for w in palette_words(p, f'scroll{layer}', 0, g)]
                         for g in range(32)] for layer in (2, 3)}
entries = []
mixed = 0
error = count = 0
lookup = {}
for ty in range(28):
    for tx in range(40):
        original = maps[(64 + ty) * 40 + tx]
        foreground = tiles[original & 32767]
        if all((b >> 4) and (b & 15) for b in foreground):
            entries.append(original)
            continue
        mixed += 1
        colors = []
        for y in range(8):
            for x in range(8):
                # Both original layer conventions reduce to screenY - 240
                # when camera Y is zero; the ROM map supplies world tiles.
                rgb = (0, 0, 0)
                for layer in (3, 2):
                    pen, group = source.pen(layer, 32 + tx * 8 + x, ty * 8 + y - 240)
                    if pen != 15:
                        rgb = source_palettes[layer][group][pen]
                colors.append(rgb)
        hist = Counter(colors)
        for color in hist:
            if color not in lookup:
                lookup[color] = [min((color_error(color, rgb), i) for i, rgb in enumerate(pal) if i)
                                 for pal in palettes]
        slot = min(range(2), key=lambda s: sum(n * lookup[c][s][0] for c, n in hist.items()))
        pens = [lookup[c][slot][1] for c in colors]
        encoded = genesis_tiles([pens[i:i + 8] for i in range(0, 64, 8)])
        if encoded not in ids:
            ids[encoded] = len(tiles)
            tiles.append(encoded)
        entries.append(ids[encoded] | (slot << 15))
        for c, pen in zip(colors, pens):
            error += sum((a - b) ** 2 for a, b in zip(c, palettes[slot][pen]))
            count += 1
(root / 'res/forest.bin').write_bytes(b''.join(tiles[base:]) or bytes(32))
(root / 'src/forest.h').write_text(
    '/* Stationary source forest; an opaque plane frees BG_B for Scroll1. */\n'
    f'#define INTRO_FOREST_TILE_COUNT {len(tiles) - base}\n'
    'static const u16 intro_forest_map[]={' + ','.join(map(str, entries)) + '};\n')
report = {'new_patterns': len(tiles) - base, 'visible_patterns': len({v & 32767 for v in entries}),
          'mixed_cells': mixed, 'preserved_opaque_foreground_cells': 1120 - mixed,
          'mixed_rgb_rmse': (error / count / 3) ** .5,
          'scope': 'Only stationary camera Y=0; mixed tiles share one of the existing two palettes'}
Path('build/intro-forest-assets.json').write_text(json.dumps(report, indent=2) + '\n')
print(report)

# Patterns needed when restoring the two original planes after dialogue.
# The native restoration may keep an existing foreground cache slot when
# the original foreground pattern matches. Its palette must match too.
for i,flat_entry in enumerate(entries):
 original=maps[64*40+i]
 if (original&32767) and (original&32767)==(flat_entry&32767):
  assert original==flat_entry, ('foreground palette alias',i,original,flat_entry)
flat={v&32767 for v in entries}
separate={v&32767 for v in maps[64*40:92*40]+maps[3680+32*40:3680+60*40]}
restore=sorted(separate-flat)
with (root/'src/forest.h').open('a') as out:
 out.write(f'#define INTRO_RESTORE_PATTERNS {len(restore)}\n')
 out.write('static const u16 intro_restore_patterns[]={'+','.join(map(str,restore))+'};\n')

changes=[];starts=[0]
for row in range(28):
 delta=Counter(v&32767 for v in maps[(64+row)*40:(65+row)*40]+maps[3680+(32+row)*40:3680+(33+row)*40])
 delta.subtract(v&32767 for v in entries[row*40:(row+1)*40])
 changes.extend(sorted(((tile,count) for tile,count in delta.items() if count),key=lambda v:(v[1]<0,v[0])))
 starts.append(len(changes))
with (root/'src/forest.h').open('a') as out:
 out.write('static const u16 intro_restore_change_start[]={'+','.join(map(str,starts))+'};\n')
 out.write('static const struct {u16 tile;short count;} intro_restore_changes[]={'+','.join('{'+str(tile)+','+str(count)+'}' for tile,count in changes)+'};\n')

# Dense ROM-only index into the transient resident-pin slots.
restore_index={tile:i for i,tile in enumerate(restore)}
with (root/'src/forest.h').open('a') as out:
 out.write('static const u16 intro_restore_index[]={'+','.join(str(restore_index.get(i,65535)) for i in range(base))+'};\n')
