#!/usr/bin/env python3
"""Convert Willow's first CPS-1 palette RAM page to Genesis CRAM colors."""

from __future__ import annotations

import argparse
from pathlib import Path

from cps1_program import assemble_maincpu

PALETTE_ROM_OFFSET = 0x0E5080
SCROLL2_PALETTE_BASE = 0x0E7D20
SCROLL3_DEFAULT_PALETTE = 0x0EAB00
SCROLL3_PALETTE_FLAGS = 0x44074
PALETTE_RUNTIME_BRIGHTNESS_OFFSET = 0x07EC
PALETTE_RUNTIME_BRIGHTNESS_BYTES = bytes.fromhex("0080f000f000")
SPRITE_PALETTE_STAGE_OFFSET = 0x0097E
SPRITE_PALETTE_STAGE_OFFSET_BYTES = bytes.fromhex("302d0344e148e348")
SPRITE_PALETTE_LOWER_BANK = 0x00986
SPRITE_PALETTE_LOWER_BANK_BYTES = bytes.fromhex(
    "43f90091c000207c000e5080323c000f")
SPRITE_PALETTE_UPPER_BANK = 0x00998
SPRITE_PALETTE_UPPER_BANK_BYTES = bytes.fromhex(
    "43f90091c200207c000e5480d1c0323c000f")
SCROLL2_STAGE_COUNT = 8
PALETTE_GROUPS = 32
COLORS_PER_GROUP = 16


def cps_rgb(word: int) -> tuple[int, int, int]:
    """Match MAME's CPS-1 brightness and xRGB444 palette conversion."""
    brightness = 0x0F + ((word >> 12) << 1)
    return tuple((((word >> shift) & 0x0F) * 0x11 * brightness // 0x2D)
                 for shift in (8, 4, 0))


def genesis_color(rgb: tuple[int, int, int]) -> int:
    """Match SGDK's rounded RGB24_TO_VDPCOLOR component packing."""
    red, green, blue = (min(7, (value + 16) >> 5) for value in rgb)
    return (red << 1) | (green << 5) | (blue << 9)


def runtime_palette_word(word: int) -> int:
    """CPS palette setup ORs full brightness into every runtime color word."""
    return word | 0xF000


def genesis_palette_channel(value: int) -> int:
    """Quantize a full-brightness CPS xRGB444 channel to Genesis RGB333."""
    return min(7, (value * 17 + 16) >> 5)


def sprite_pen_map(data: bytes, stage: int, source_group: int,
                   target_group: int = 16) -> list[int]:
    """Map a CPS sprite palette to the closest Genesis RGB333 palette."""
    source = palette_words(data, "base", stage=stage, group=source_group)
    target = palette_words(data, "base", stage=stage, group=target_group)

    def quantized(word: int) -> tuple[int, int, int]:
        word = runtime_palette_word(word)
        return tuple(genesis_palette_channel((word >> shift) & 15)
                     for shift in (8, 4, 0))

    target_rgb = [quantized(word) for word in target[:15]]
    result = [0]  # CPS sprite pen 15 is transparent and packs to Genesis pen 0.
    for source_index in range(15):
        color = quantized(source[source_index])
        best_index = min(
            range(15),
            key=lambda index: sum((color[channel] - target_rgb[index][channel]) ** 2
                                  for channel in range(3)),
        )
        result.append(best_index + 1)
    return result


def palette_words(data: bytes, layer: str, stage: int = 0,
                  group: int = 0) -> list[int]:
    if layer == "base":
        if not 0 <= stage < SCROLL2_STAGE_COUNT or not 0 <= group < PALETTE_GROUPS:
            raise ValueError("base palette stage/group is out of range")
        # Sprite palettes are copied in two 16-group banks by startup routine
        # 0x978: groups 0-15 start at 0x0e5080, groups 16-31 at 0x0e5480.
        # The stage index advances each source bank by 0x200 bytes.
        base = (PALETTE_ROM_OFFSET + stage * 0x200
                + (0x400 if group >= 16 else 0))
        palette_group = group & 15
    elif layer == "scroll2":
        base = SCROLL2_PALETTE_BASE + stage * 0x400
        palette_group = group
    elif layer == "scroll1":
        # 9CA..9D6 loads a fixed bank. Unlike Scroll2, there is no ADDA D0
        # before the copy at 9F8. All stage intro text uses this ROM bank.
        if not 0 <= group < PALETTE_GROUPS:
            raise ValueError("scroll1 palette group is out of range")
        base = 0x0E6EA0
        palette_group = group
    elif layer == "scroll3":
        flag = data[SCROLL3_PALETTE_FLAGS + stage]
        override_groups = 32 if flag == 0xFF else 16
        override_destination = 16 if flag == 1 else 0
        base = int.from_bytes(data[0x44050 + stage * 4:
                                   0x44054 + stage * 4], "big")
        if not (override_destination <= group
                < override_destination + override_groups):
            base = SCROLL3_DEFAULT_PALETTE
            palette_group = group
        else:
            palette_group = group - override_destination
    else:
        raise ValueError(f"unknown palette layer {layer}")
    offset = base + palette_group * COLORS_PER_GROUP * 2
    return [int.from_bytes(data[offset + index * 2:offset + index * 2 + 2],
                           "big") for index in range(COLORS_PER_GROUP)]


def to_header(data: bytes) -> str:
    palette_copy_checks = (
        (SPRITE_PALETTE_STAGE_OFFSET, SPRITE_PALETTE_STAGE_OFFSET_BYTES,
         "sprite palette stage offset"),
        (SPRITE_PALETTE_LOWER_BANK, SPRITE_PALETTE_LOWER_BANK_BYTES,
         "lower sprite palette bank"),
        (SPRITE_PALETTE_UPPER_BANK, SPRITE_PALETTE_UPPER_BANK_BYTES,
         "upper sprite palette bank"),
    )
    for address, expected, label in palette_copy_checks:
        if data[address:address + len(expected)] != expected:
            raise ValueError(f"CPS {label} routine at {address:#x} changed")
    end = PALETTE_RUNTIME_BRIGHTNESS_OFFSET + len(PALETTE_RUNTIME_BRIGHTNESS_BYTES)
    if data[PALETTE_RUNTIME_BRIGHTNESS_OFFSET:end] != PALETTE_RUNTIME_BRIGHTNESS_BYTES:
        raise ValueError("CPS palette runtime brightness initializer changed")
    rows = [
        "/* Generated by tools/cps1_palette.py from Willow CPS palette ROM data. */",
        "static const u16 willow_cps_palette_words[8][32][16] = {",
    ]
    for stage in range(SCROLL2_STAGE_COUNT):
        rows.append("    {")
        for group in range(PALETTE_GROUPS):
            rows.append("        { " + ", ".join(
                f"0x{word:04x}" for word in palette_words(
                    data, "base", stage=stage, group=group)) + " },")
        rows.append("    },")
    rows.extend([
        "};",
        "static const u8 willow_linked_sprite_pen_map[8][16] = {",
    ])
    for stage in range(SCROLL2_STAGE_COUNT):
        rows.append("    { " + ", ".join(
            str(value) for value in sprite_pen_map(data, stage, 31)) + " },")
    rows.extend([
        "};",
        "static const u8 willow_post_phase_sprite_pen_map[8][16] = {",
    ])
    for stage in range(SCROLL2_STAGE_COUNT):
        rows.append("    { " + ", ".join(
            str(value) for value in sprite_pen_map(data, stage, 8)) + " },")
    rows.extend([
        "};",
        "static const u16 willow_stage_scroll2_palette_words[8][32][16] = {",
    ])
    for stage in range(SCROLL2_STAGE_COUNT):
        rows.append("    {")
        for group in range(PALETTE_GROUPS):
            words = palette_words(data, "scroll2", stage, group)
            rows.append("        { " + ", ".join(
                f"0x{word:04x}" for word in words) + " },")
        rows.append("    },")
    rows.extend([
        "};",
        "static const u16 willow_stage_scroll3_palette_words[8][32][16] = {",
    ])
    for stage in range(SCROLL2_STAGE_COUNT):
        rows.append("    {")
        for group in range(PALETTE_GROUPS):
            words = palette_words(data, "scroll3", stage, group)
            rows.append("        { " + ", ".join(
                f"0x{word:04x}" for word in words) + " },")
        rows.append("    },")
    rows.extend([
        "};",
        "static const u16 willow_cps_palettes[32][16] = {",
    ])
    for group in range(PALETTE_GROUPS):
        colors: list[str] = []
        for genesis_index in range(COLORS_PER_GROUP):
            # CPS sprites use source pen 15 as transparent. Sprite graphics
            # rotate source pens by one when packed, so Genesis pen 0 stays
            # transparent and output pens 1..15 use source pens 0..14.
            if genesis_index == 0:
                colors.append("RGB24_TO_VDPCOLOR(0x000000)")
                continue
            source_index = genesis_index - 1
            offset = PALETTE_ROM_OFFSET + (group * COLORS_PER_GROUP + source_index) * 2
            word = int.from_bytes(data[offset : offset + 2], "big")
            red, green, blue = cps_rgb(runtime_palette_word(word))
            colors.append(f"RGB24_TO_VDPCOLOR(0x{red:02x}{green:02x}{blue:02x})")
        rows.append("    { " + ", ".join(colors) + " },")
    rows.append("};")
    stage_palette_colors: list[list[list[int]]] = [
        [[] for _ in range(64)] for _ in range(SCROLL2_STAGE_COUNT)
    ]
    for layer_index, (layer, pointer_table, default_base) in enumerate((
        ("scroll2", None, SCROLL2_PALETTE_BASE),
        ("scroll3", 0x44050, None),
    )):
        rows.append(f"static const u16 willow_stage_{layer}_palettes[8][32][16] = {{")
        for stage in range(SCROLL2_STAGE_COUNT):
            base = (default_base + stage * 0x400 if default_base is not None
                    else int.from_bytes(
                        data[pointer_table + stage * 4:pointer_table + stage * 4 + 4],
                        "big"))
            override_groups = 0
            override_destination = 0
            if layer == "scroll3":
                flag = data[SCROLL3_PALETTE_FLAGS + stage]
                override_groups = 32 if flag == 0xFF else 16
                override_destination = 16 if flag == 1 else 0
            rows.append("    {")
            for group in range(PALETTE_GROUPS):
                colors = []
                for index in range(COLORS_PER_GROUP):
                    palette_base = base
                    palette_group = group
                    if layer == "scroll3" and not (
                            override_destination <= group
                            < override_destination + override_groups):
                        palette_base = SCROLL3_DEFAULT_PALETTE
                    elif layer == "scroll3":
                        palette_group = group - override_destination
                    offset = palette_base + (palette_group * 16 + ((index - 1) & 15)) * 2
                    word = int.from_bytes(data[offset : offset + 2], "big")
                    if index == 0:
                        colors.append("RGB24_TO_VDPCOLOR(0x000000)")
                        stage_palette_colors[stage][layer_index * 32 + group].append(0)
                    else:
                        red, green, blue = cps_rgb(runtime_palette_word(word))
                        colors.append(f"RGB24_TO_VDPCOLOR(0x{red:02x}{green:02x}{blue:02x})")
                        stage_palette_colors[stage][layer_index * 32 + group].append(
                            genesis_color((red, green, blue)))
                rows.append("        { " + ", ".join(colors) + " },")
            rows.append("    },")
        rows.append("};")
    rows.append("static const u16 willow_stage_palette_distances[8][64][64] = {")
    for stage in range(SCROLL2_STAGE_COUNT):
        rows.append("    {")
        palettes = stage_palette_colors[stage]
        for left in palettes:
            distances = []
            for right in palettes:
                distance = 0
                for color in range(1, COLORS_PER_GROUP):
                    a, b = left[color], right[color]
                    distance += abs(((a >> 1) & 7) - ((b >> 1) & 7))
                    distance += abs(((a >> 5) & 7) - ((b >> 5) & 7))
                    distance += abs(((a >> 9) & 7) - ((b >> 9) & 7))
                distances.append(f"{distance}")
            rows.append("        { " + ", ".join(distances) + " },")
        rows.append("    },")
    rows.append("};")
    return "\n".join(rows) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_palettes.h"))
    args = parser.parse_args()
    try:
        cpu_region = assemble_maincpu(args.archive)
    except (OSError, KeyError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(to_header(cpu_region), encoding="ascii")
    print(f"Converted base, Scroll 2, and Scroll 3 palettes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
