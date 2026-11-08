#!/usr/bin/env python3
"""Render one indexed Scroll 2 or Scroll 3 source map chunk as a PPM preview."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

from cps1_gfx import assemble_region, decode_tile
from cps1_program import assemble_maincpu

SCROLL2_PALETTE_BASE = 0x0E7D20
SCROLL2_PALETTE_LIMIT = 0x0EAB00
SCROLL3_DEFAULT_PALETTE = 0x0EAB00
SCROLL3_PALETTE_POINTERS = 0x44050
SCROLL3_PALETTE_FLAGS = 0x44074
SCROLL3_PALETTE_COUNT = 8


def cps_rgb(word: int) -> tuple[int, int, int]:
    brightness = 0x0F + ((word >> 12) << 1)
    return tuple((((word >> shift) & 0x0F) * 0x11 * brightness // 0x2D)
                 for shift in (8, 4, 0))


def source_palette(program: bytes, layer: str, palette_index: int) -> list[tuple[int, int, int]]:
    if layer == "scroll2":
        if not 0 <= palette_index < SCROLL2_PALETTE_COUNT:
            raise ValueError(f"Scroll 2 palette index must be 0..{SCROLL2_PALETTE_COUNT - 1}")
        start = SCROLL2_PALETTE_BASE + palette_index * 0x400
    else:
        if not 0 <= palette_index < SCROLL3_PALETTE_COUNT:
            raise ValueError(f"Scroll 3 palette index must be 0..{SCROLL3_PALETTE_COUNT - 1}")
        pointer_at = SCROLL3_PALETTE_POINTERS + palette_index * 4
        start = int.from_bytes(program[pointer_at : pointer_at + 4], "big")
        flag = program[SCROLL3_PALETTE_FLAGS + palette_index]
        default = program[SCROLL3_DEFAULT_PALETTE : SCROLL3_DEFAULT_PALETTE + 0x400]
        selected = program[start : start + 0x400]
        if len(default) != 0x400 or len(selected) != 0x400:
            raise ValueError(f"palette block at {start:#x} is outside the source ROM")
        combined = bytearray(default)
        groups = 32 if flag == 0xFF else 16
        destination_group = 16 if flag == 1 else 0
        combined[destination_group * 32 : (destination_group + groups) * 32] = selected[:groups * 32]
        raw = bytes(combined)
    if layer == "scroll2":
        raw = program[start : start + 0x400]
    if len(raw) != 0x400:
        raise ValueError(f"palette block at {start:#x} is outside the source ROM")
    return [cps_rgb(int.from_bytes(raw[i : i + 2], "big"))
            for i in range(0, len(raw), 2)]


SCROLL2_PALETTE_COUNT = (SCROLL2_PALETTE_LIMIT - SCROLL2_PALETTE_BASE) // 0x400


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("layer", choices=("scroll2", "scroll3"))
    parser.add_argument("index", type=int)
    parser.add_argument("--archive", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--palette-index", type=int, default=0,
                        help="source palette set (Scroll 2: 0..10; Scroll 3: 0..7)")
    args = parser.parse_args()
    if not 0 <= args.index <= 255:
        parser.error("index must be in 0..255")
    base, stride, size, columns, rows, tile_base, tile_mask = (
        (0x80000, 0x400, 16, 16, 16, 0x4000, 0x1FFF)
        if args.layer == "scroll2" else
        (0xC0000, 0x100, 32, 8, 8, 0x0A00, 0xFFFF)
    )
    with zipfile.ZipFile(args.archive) as zf:
        program = assemble_maincpu(args.archive)
        gfx = assemble_region(zf)
    try:
        palette = source_palette(program, args.layer, args.palette_index)
    except ValueError as exc:
        parser.error(str(exc))
    start = base + args.index * stride
    # Each CPS map cell is a two-word (code, attributes) pair. The ROM copy
    # routines preserve both words, while only the first selects graphics.
    count = columns * rows
    cells = []
    for cell in range(count):
        at = start + cell * 4
        word = int.from_bytes(program[at : at + 2], "big")
        attribute = int.from_bytes(program[at + 2 : at + 4], "big")
        code = tile_base + (word & tile_mask)
        # The 4 MiB graphics region contains 0x2000 decoded 32x32 tiles.
        # Out-of-region codes read no physical graphics and render transparent.
        valid = args.layer != "scroll3" or 0x0A00 <= code < 0x2000
        cells.append((code if valid else None, attribute))
    width, height = columns * size, rows * size
    pixels = bytearray(width * height * 3)
    tiles = {code: decode_tile(gfx, code, size)
             for code in {code for code, _ in cells if code is not None}}
    for tile_index, (code, attribute) in enumerate(cells):
        # CPS-1 layer maps scan tile RAM column first: row is the low-order
        # address, followed by the column.
        x, y = tile_index // rows, tile_index % rows
        tile = tiles[code] if code is not None else None
        flip_x = bool(attribute & 0x0020)
        flip_y = bool(attribute & 0x0040)
        for py in range(size):
            for px in range(size):
                pen = 0 if tile is None else tile[size - 1 - py if flip_y else py][
                    size - 1 - px if flip_x else px
                ]
                rgb = palette[(attribute & 0x1F) * 16 + pen]
                dst = ((y * size + py) * width + x * size + px) * 3
                pixels[dst : dst + 3] = bytes(rgb)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)
    print(f"Rendered {args.layer} chunk {args.index} ({columns}x{rows} tiles) to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
