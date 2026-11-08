#!/usr/bin/env python3
"""Decode Willow's CPS-1 graphics into Genesis 8x8, 4bpp tile order.

The input is MAME's ROM set, not executable arcade code. This reproduces the
pixel bit layout described by MAME's CPS-1 graphics layout and writes native
Mega Drive tile bytes for the selected 16x16 CPS sprite tiles.
"""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

WORD_LANES = ("wlm-7.7a", "wlm-5.9a", "wlm-3.3a", "wlm-1.5a")
BYTE_LANES = (
    "wl_24.7d", "wl_14.7c", "wl_26.9d", "wl_16.9c",
    "wl_20.3d", "wl_10.3c", "wl_22.5d", "wl_12.5c",
)
GFX_REGION_SIZE = 0x400000
TILE_BYTES = 128  # decoded 16x16 pixels at 4bpp
TILE_COUNT = GFX_REGION_SIZE // TILE_BYTES


def assemble_region(zf: zipfile.ZipFile) -> bytearray:
    """Apply Willow's ROM_LOAD64_WORD/BYTE lane placement to a 4 MiB region."""
    region = bytearray(GFX_REGION_SIZE)
    words = [zf.read(name) for name in WORD_LANES]
    if any(len(data) != 0x80000 for data in words):
        raise ValueError("Unexpected size in the four 512 KiB graphics word ROMs")
    for offset in range(0, 0x200000, 8):
        src = offset // 4
        for lane, data in enumerate(words):
            region[offset + lane * 2 : offset + lane * 2 + 2] = data[src : src + 2]

    bytes_ = [zf.read(name) for name in BYTE_LANES]
    if any(len(data) != 0x20000 for data in bytes_):
        raise ValueError("Unexpected size in the eight 128 KiB graphics byte ROMs")
    for lane, data in enumerate(bytes_):
        region[0x200000 + lane : 0x300000 : 8] = data
    return region


def bit(data: bytearray, bit_offset: int) -> int:
    return (data[bit_offset >> 3] >> (7 - (bit_offset & 7))) & 1


def decode_tile(region: bytearray, code: int, size: int,
                scroll1_layout: int = 0) -> list[list[int]]:
    """Decode a CPS 8x8, 16x16, or 32x32 tile to palette indices."""
    if size not in (8, 16, 32):
        raise ValueError(f"unsupported CPS tile size: {size}")
    # CPS's 8x8 layouts have a 64-byte character increment: the two layout
    # variants select opposite 32-byte halves of each 16x8 source region.
    tile_increment = 512 if size == 8 else size * size * 4
    row_increment = 64 if size == 8 else size * 4
    base = code * tile_increment
    pixels = [[0] * size for _ in range(size)]
    # MAME's layouts use 8-pixel chunks, with the two 8x8 layouts selecting
    # the left and right halves of each 16x8 source region.
    for y in range(size):
        row = base + y * row_increment
        for x in range(size):
            xoff = (x & 7) + (x // 8) * 32
            if size == 8:
                xoff += scroll1_layout * 32
            value = 0
            for plane, source_bit in enumerate((24, 16, 8, 0)):
                value |= bit(region, row + xoff + source_bit) << (3 - plane)
            pixels[y][x] = value
    return pixels


def genesis_tiles(pixels: list[list[int]]) -> bytes:
    """Convert one CPS tile into Genesis 8x8 planar tiles."""
    size = len(pixels)
    if size not in (8, 16, 32) or any(len(row) != size for row in pixels):
        raise ValueError("pixel array must be square with size 8, 16, or 32")
    out = bytearray(size * size // 2)
    tile_index = 0
    for tile_y in range(0, size, 8):
        for tile_x in range(0, size, 8):
            dst = tile_index * 32
            for y in range(8):
                for x in range(8):
                    color = pixels[tile_y + y][tile_x + x]
                    at = dst + y * 4 + x // 2
                    if x & 1:
                        out[at] |= color
                    else:
                        out[at] = color << 4
            tile_index += 1
    return bytes(out)


def write_preview(path: Path, tiles: list[list[list[int]]], columns: int = 16) -> None:
    tile_size = len(tiles[0])
    width, height = columns * tile_size, ((len(tiles) + columns - 1) // columns) * tile_size
    rgb = bytearray([20] * (width * height * 3))
    palette = [(0, 0, 0), (255, 255, 255), (180, 180, 180), (96, 96, 96),
               (255, 96, 64), (64, 192, 255), (255, 224, 64), (96, 224, 96),
               (224, 96, 224), (96, 96, 255), (255, 160, 64), (160, 255, 224),
               (224, 224, 224), (128, 64, 32), (64, 128, 128), (32, 32, 32)]
    for index, tile in enumerate(tiles):
        ox, oy = (index % columns) * tile_size, (index // columns) * tile_size
        for y in range(tile_size):
            for x in range(tile_size):
                color = palette[tile[y][x]]
                pos = ((oy + y) * width + ox + x) * 3
                rgb[pos : pos + 3] = bytes(color)
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + rgb)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--first", type=int, default=0, help="first CPS logical tile code")
    parser.add_argument("--count", type=int, default=256, help="number of CPS tiles to decode")
    parser.add_argument("--layer", choices=("sprites", "scroll1", "scroll2", "scroll3"),
                        default="sprites", help="CPS graphics mapping to decode")
    parser.add_argument("--scroll1-layout", type=int, choices=(0, 1), default=0,
                        help="8x8 left/right half selected by the Scroll 1 tilemap index")
    parser.add_argument("--output", type=Path, default=Path("build/gfx-tiles.bin"))
    parser.add_argument("--preview", type=Path, help="optional PPM tile contact sheet")
    args = parser.parse_args()
    if args.count < 1:
        parser.error("count must be positive")
    if args.layer == "sprites":
        size = 16
        first_physical = args.first
        if args.first < 0 or args.first + args.count > TILE_COUNT:
            parser.error(f"sprite range must be within 0..{TILE_COUNT - 1}")
    elif args.layer == "scroll1":
        size = 8
        if args.first < 0x7000 or args.first + args.count > 0x8000:
            parser.error("Willow Scroll 1 codes must be within 0x7000..0x7fff")
        first_physical = args.first
    elif args.layer == "scroll2":
        size = 16
        if args.first < 0 or args.first + args.count > 0x2000:
            parser.error("Willow Scroll 2 codes must be within 0..0x1fff")
        # WL24B maps Scroll 2 code c to the 16x16 decoded ROM tile 0x4000+c.
        first_physical = args.first + 0x4000
    else:
        size = 32
        if args.first < 0x0A00 or args.first + args.count > 0x2000:
            parser.error("Willow Scroll 3 codes must be within 0x0a00..0x1fff")
        # WL24B's Scroll 3 mapper uses code<<3 for range decode and shifts the
        # mapped address back, yielding the same decoded tile code.
        first_physical = args.first
    try:
        with zipfile.ZipFile(args.archive) as zf:
            region = assemble_region(zf)
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    preview: list[list[list[int]]] = []
    with args.output.open("wb") as output:
        for code in range(first_physical, first_physical + args.count):
            pixels = decode_tile(region, code, size, args.scroll1_layout)
            output.write(genesis_tiles(pixels))
            if args.preview:
                preview.append(pixels)
    if args.preview:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        write_preview(args.preview, preview)
    print(f"Decoded {args.layer} CPS {size}x{size} tiles "
          f"{args.first}..{args.first + args.count - 1} to {args.output} "
          f"({args.count * size * size // 2:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
