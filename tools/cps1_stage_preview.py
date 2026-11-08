#!/usr/bin/env python3
"""Compose a Willow gameplay stage from its native map chunk selectors."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

from cps1_gfx import assemble_region, decode_tile
from cps1_layer_map_preview import source_palette
from cps1_program import assemble_maincpu
from cps1_stage_catalog import catalog
from cps1_gameplay_maps import gameplay_map


def render(program: bytes, gfx: bytes, stage: int, layer: str) -> tuple[int, int, bytes, tuple[int, int]]:
    # The two tables describe different layers, not different game modes.
    if layer == 'scroll3':
        if stage != 0:
            raise ValueError('Scroll3 grid modes beyond stage 0 still require code validation')
        stage_data = catalog(program)['stages'][stage]
        columns, rows = 16, 16
    else:
        stage_data = gameplay_map(program,stage)
        columns, rows = stage_data['columns'], stage_data['rows']
    source = stage_data["chunk_ids"]
    # Source chunk Y grows upward, while tile pixels within a chunk grow down.
    selectors = [c for row in range(rows-1,-1,-1) for c in source[row*columns:(row+1)*columns]]
    nonzero = [(i % columns, i // columns) for i, value in enumerate(selectors) if value]
    if not nonzero:
        return 256, 256, bytes(256 * 256 * 3), (0, 0)
    min_x = min(x for x, _ in nonzero)
    max_x = max(x for x, _ in nonzero)
    min_y = min(y for _, y in nonzero)
    max_y = max(y for _, y in nonzero)

    if layer == "scroll2":
        map_base, stride, cell_columns, cell_rows, tile_size, tile_base, tile_mask = (
            0x80000, 0x400, 16, 16, 16, 0x4000, 0x1FFF
        )
    else:
        map_base, stride, cell_columns, cell_rows, tile_size, tile_base, tile_mask = (
            0xC0000, 0x100, 8, 8, 32, 0x0A00, 0xFFFF
        )

    chunk_pixels = cell_columns * tile_size
    width = (max_x - min_x + 1) * chunk_pixels
    height = (max_y - min_y + 1) * chunk_pixels
    pixels = bytearray(width * height * 3)
    palette = source_palette(program, layer, stage)
    tile_cache: dict[int, list[list[int]]] = {}

    for grid_y in range(min_y, max_y + 1):
        for grid_x in range(min_x, max_x + 1):
            chunk = selectors[grid_y * columns + grid_x]
            if chunk == 0:
                continue
            chunk_start = map_base + chunk * stride
            chunk_origin_x = (grid_x - min_x) * chunk_pixels
            chunk_origin_y = (grid_y - min_y) * chunk_pixels
            cell_count = cell_columns * cell_rows
            for cell_index in range(cell_count):
                at = chunk_start + cell_index * 4
                code_word = int.from_bytes(program[at : at + 2], "big")
                attribute = int.from_bytes(program[at + 2 : at + 4], "big")
                code = tile_base + (code_word & tile_mask)
                if layer == "scroll3" and not 0x0A00 <= code <= 0x0DFF:
                    continue
                if code not in tile_cache:
                    tile_cache[code] = decode_tile(gfx, code, tile_size)
                tile = tile_cache[code]
                # CPS-1 maps scan tile RAM column first: row occupies the
                # low address bits, followed by the column.
                cell_x = chunk_origin_x + (cell_index // cell_rows) * tile_size
                cell_y = chunk_origin_y + (cell_index % cell_rows) * tile_size
                flip_x = bool(attribute & 0x0020)
                flip_y = bool(attribute & 0x0040)
                palette_start = (attribute & 0x1F) * 16
                for py in range(tile_size):
                    sy = tile_size - 1 - py if flip_y else py
                    for px in range(tile_size):
                        sx = tile_size - 1 - px if flip_x else px
                        color = palette[palette_start + tile[sy][sx]]
                        dst = ((cell_y + py) * width + cell_x + px) * 3
                        pixels[dst : dst + 3] = bytes(color)
    return width, height, bytes(pixels), (min_x * chunk_pixels, min_y * chunk_pixels)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", type=int, choices=range(8))
    parser.add_argument("layer", choices=("scroll2", "scroll3"))
    parser.add_argument("--archive", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with zipfile.ZipFile(args.archive) as zf:
        program = assemble_maincpu(args.archive)
        gfx = assemble_region(zf)
    width, height, pixels, origin = render(program, gfx, args.stage, args.layer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)
    print(f"Rendered stage {args.stage} {args.layer}: {width}x{height} pixels, source origin {origin}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
