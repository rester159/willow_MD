#!/usr/bin/env python3
"""Extract uncompressed source Scroll 1 glyphs for a native HUD renderer."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


SOURCE_TILE_BYTES = 32  # one 8x8 4bpp glyph in Genesis tile order
SOURCE_LAYOUT_BYTES = 0x1000 * SOURCE_TILE_BYTES
ASCII_FIRST = 0x20
ASCII_COUNT = 0x40  # space through underscore: punctuation, digits, A-Z
FRONTEND_GLYPH_COUNT = 0x100


def extract(source: Path, output: Path) -> dict[str, object]:
    data = source.read_bytes()
    if len(data) != SOURCE_LAYOUT_BYTES:
        raise ValueError(
            f"{source} must contain 4096 uncompressed Scroll 1 tiles "
            f"({SOURCE_LAYOUT_BYTES} bytes), found {len(data)}"
        )
    start = ASCII_FIRST * SOURCE_TILE_BYTES
    end = (ASCII_FIRST + ASCII_COUNT) * SOURCE_TILE_BYTES
    output.parent.mkdir(parents=True, exist_ok=True)
    # CPS Scroll 1 uses source pen 15 as the transparent pixel, while the
    # Genesis VDP reserves pen 0 for transparency. Rotate the source pens so
    # source colors 0..14 remain addressable as Genesis colors 1..15.
    glyphs = bytearray(data[start:end])
    for index, value in enumerate(glyphs):
        high = ((value >> 4) + 1) & 15
        low = ((value & 15) + 1) & 15
        glyphs[index] = (high << 4) | low
    output.write_bytes(glyphs)
    return {
        "source": str(source),
        "output": str(output),
        "source_code_first": f"0x{0x7000 + ASCII_FIRST:04x}",
        "ascii_first": ASCII_FIRST,
        "ascii_count": ASCII_COUNT,
        "tile_bytes": SOURCE_TILE_BYTES,
        "bytes": end - start,
        "compression": "none",
        "pixel_mapping": "Genesis pen = (CPS Scroll 1 pen + 1) modulo 16; source pen 15 becomes transparent pen 0",
    }


def extract_frontend(source: Path, output: Path) -> dict[str, object]:
    """Keep all byte-addressed Scroll 1 glyphs for native front-end text."""
    data = source.read_bytes()
    if len(data) != SOURCE_LAYOUT_BYTES:
        raise ValueError(
            f"{source} must contain 4096 uncompressed Scroll 1 tiles "
            f"({SOURCE_LAYOUT_BYTES} bytes), found {len(data)}"
        )
    glyphs = bytearray(data[:FRONTEND_GLYPH_COUNT * SOURCE_TILE_BYTES])
    for index, value in enumerate(glyphs):
        high = ((value >> 4) + 1) & 15
        low = ((value & 15) + 1) & 15
        glyphs[index] = (high << 4) | low
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(glyphs)
    return {
        "source": str(source),
        "output": str(output),
        "source_code_first": "0x7000",
        "glyph_code_count": FRONTEND_GLYPH_COUNT,
        "tile_bytes": SOURCE_TILE_BYTES,
        "bytes": len(glyphs),
        "compression": "none",
        "pixel_mapping": "Genesis pen = (CPS Scroll 1 pen + 1) modulo 16; source pen 15 becomes transparent pen 0",
    }


def extract_timer_digits(left: Path, right: Path, output: Path) -> dict[str, object]:
    """Pack both alternating Scroll 1 layouts for digits 0 through 9."""
    tiles = bytearray()
    for source in (left, right):
        data = source.read_bytes()
        if len(data) != ASCII_COUNT * SOURCE_TILE_BYTES:
            raise ValueError(f"{source} must contain 64 source glyph tiles")
        first_digit = (ord("0") - ASCII_FIRST) * SOURCE_TILE_BYTES
        tiles.extend(data[first_digit:first_digit + 10 * SOURCE_TILE_BYTES])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(tiles)
    return {
        "output": str(output),
        "layouts": ["left/even Scroll 1 map columns", "right/odd Scroll 1 map columns"],
        "digits": "0-9",
        "tile_bytes": SOURCE_TILE_BYTES,
        "bytes": len(tiles),
        "compression": "none",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--left", type=Path, default=Path("build/willow_scroll1_left.bin"))
    parser.add_argument("--right", type=Path, default=Path("build/willow_scroll1_right.bin"))
    parser.add_argument("--output-dir", type=Path, default=Path("build"))
    args = parser.parse_args()
    try:
        left = extract(args.left, args.output_dir / "willow_hud_font_left.bin")
        right = extract(args.right, args.output_dir / "willow_hud_font_right.bin")
        timer_digits = extract_timer_digits(
            args.output_dir / "willow_hud_font_left.bin",
            args.output_dir / "willow_hud_font_right.bin",
            args.output_dir / "willow_hud_timer_digits.bin")
        frontend = extract_frontend(
            args.left, args.output_dir / "willow_frontend_font.bin")
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    manifest = {
        "source_glyph_writer": "0x7b1a maps character bytes to 0x7000 + character",
        "alternate_glyph_writer": "0x7b34 maps character bytes to 0x7100 + character",
        "source_layouts": [left, right],
        "timer_digits": timer_digits,
        "front_end_glyphs": frontend,
        "limitations": [
            "Only ASCII 0x20-0x5f is extracted; message records outside this range need additional glyphs.",
        ],
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "willow_hud_font.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print("Extracted 64 glyphs per Scroll 1 layout and both ten-digit timer fonts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
