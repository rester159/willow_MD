#!/usr/bin/env python3
"""Build a ROM-derived Stage 0 collision window and verify its visible cells.
The full 1024-pixel window is a test fixture, not a replacement for the arcade
map-streaming scheduler. Only the loaded forest viewport is trace-verified.
"""
from pathlib import Path
from cps1_source_layers import SourceLayers

program = Path('build/reference_maincpu.bin').read_bytes()
source = SourceLayers(program, None)
window = bytearray(16384)
for y in range(0, 1024, 16):
    for x in range(0, 1024, 16):
        inverse = (~y) & 65535
        offset = ((x & 0x3f0) << 2) + ((inverse & 0x300) << 4) + ((inverse & 0xf0) >> 2)
        code, attribute = source.record(2, x, inverse)
        window[offset:offset + 4] = code.to_bytes(2, 'big') + attribute.to_bytes(2, 'big')

capture = Path('build/arcade-trace/intro-vram-1530.bin').read_bytes()
assert len(capture) == 0x30000
checked = 0
for y in range(0, 256, 16):
    for x in range(0, 384, 16):
        inverse = (~y) & 65535
        offset = ((x & 0x3f0) << 2) + ((inverse & 0x300) << 4) + ((inverse & 0xf0) >> 2)
        # The collision routines consume the attribute's high byte, including
        # collision type bits 2..7. Preserve the two other bits as well.
        assert window[offset + 2] == capture[0x18000 + offset + 2], (x, y)
        checked += 1
Path('build/stage0-collision-window.bin').write_bytes(window)
print(f'Stage 0 forest: {checked} ROM-derived collision attributes match captured Scroll2 RAM')
