#!/usr/bin/env python3
"""Compare ROM-extracted panel/title pixels with headless arcade screenshots.

The reference inputs are the arcade's rendered RGB image, live tilemap and
live palette. The candidate uses the native packed asset bank and original
ROM palette; neither the generator's graphics decoder nor packing function
is used here. Transparent pixels are excluded from this layer-only check.
"""
from pathlib import Path
import json
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_palette import cps_rgb

program = assemble_maincpu(Path('willow.zip'))
assert program[0x9ca:0x9d8] == bytes.fromhex('43f90091c400207c000e6ea06120')
root = Path('build/arcade-trace/actor-native')
assets = json.loads(Path('build/intro-scroll1-assets.json').read_text())
codes = {code: (left, right) for code, left, right in assets['codes']}
palettes = dict(zip(assets['groups'], assets['cps_palettes']))
tiles = Path('native/intro_probe/res/scroll1.bin').read_bytes()
assert len(tiles) == assets['patterns'] * 32
report = []
for frame in (1154, 1158, 1162, 1166, 1170, 1320, 1490, 1500, 1520, 1540, 1580):
    ram = (root / f'text-{frame}.bin').read_bytes()
    palette = (root / f'palette-{frame}.bin').read_bytes()
    actual = Image.open(root / f'scroll1-{frame}.png').convert('RGB')
    assert actual.size == (384, 224)
    for group, words in palettes.items():
        for i, word in enumerate(words):
            at = group * 32 + i * 2
            assert word == int.from_bytes(palette[at:at + 2], 'big'), (frame, group, i)
    count = 0
    patterns = set()
    for col in range(48):
        for row in range(2, 30):
            at = col * 128 + row * 4
            code = int.from_bytes(ram[at:at + 2], 'big')
            attr = int.from_bytes(ram[at + 2:at + 4], 'big')
            assert code in codes, (frame, col, row, hex(code))
            tile = codes[code][col & 1]
            if tile:
                patterns.add(tile)
            for y in range(8):
                for x in range(8):
                    xx = 7 - x if attr & 32 else x
                    yy = 7 - y if attr & 64 else y
                    b = tiles[tile * 32 + yy * 4 + xx // 2]
                    pen = b & 15 if xx & 1 else b >> 4
                    if not pen:
                        continue
                    color = cps_rgb(palettes[attr & 31][pen - 1])
                    position = col * 8 + x, row * 8 - 16 + y
                    assert actual.getpixel(position) == color, (frame, position, hex(code), hex(attr))
                    count += 1
    assert count
    report.append({'frame': frame, 'opaque_pixels': count, 'visible_patterns': len(patterns)})
result = {'frames': report, 'opaque_pixels': sum(r['opaque_pixels'] for r in report),
          'palette_words_checked': len(report) * len(palettes) * 16,
          'scope': 'Original Scroll1 layer pixels; not Genesis compositing or palette fitting'}
Path('build/intro-scroll1-pixel-parity.json').write_text(json.dumps(result, indent=2) + '\n')
print(f"Scroll1 assets: {result['opaque_pixels']} original pixels and {result['palette_words_checked']} palette words match {len(report)} arcade images")
