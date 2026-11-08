#!/usr/bin/env python3
"""Pack uncompressed graphics referenced by Willow animations and stages."""

from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

from cps1_animation_catalog import build_catalog
from cps1_gfx import assemble_region, decode_tile, genesis_tiles
from cps1_program import assemble_maincpu
from cps1_stage_catalog import catalog as stage_catalog


def block_codes(code: int, width: int, height: int) -> set[int]:
    return {
        (code & 0xFFF0) + ((code + column) & 0x000F) + (row << 4)
        for row in range(height)
        for column in range(width)
    }


def referenced_sprite_codes(animations: dict) -> set[int]:
    # Retain the entire 0x8000-address CPS graphics space without compression.
    # This preserves the complete graphics address space for newly traced
    # native frame descriptors; runtime VRAM streams only active frames.
    return set(range(0x8000))


def prototype_sprite_codes(animations: dict) -> set[int]:
    """Tiles for the currently implemented Willow and first-enemy sequences."""
    selected = {"willow_main", "player_aux_d1a2", "player_aux_d1aa",
                "player_aux_d1a6", "player_aux_d1ae",
                "player_aux_d1b2", "player_aux_d1b6", "player_aux_d2ca",
                "player_aux_d27a", "player_aux_d2ce", "player_aux_d3be", "player_aux_d3c2",
                "source_table_01116e", "enemy_return_1136e",
                "source_table_0113fa", "source_table_011416"}
    codes: set[int] = set()
    for name in selected:
        for sequence in animations["tables"][name]["sequences"]:
            for record in sequence["records"]:
                words = [int(word, 16) for word in record["words"]]
                descriptor, code = words[1], words[5]
                width = ((descriptor >> 8) & 0x0F) + 1
                height = ((descriptor >> 12) & 0x0F) + 1
                codes.update(code for code in block_codes(code, width, height)
                             if 0 <= code < 0x8000)
    return codes


def referenced_layer_codes(program: bytes, stages: dict) -> tuple[set[int], set[int]]:
    scroll2: set[int] = set()
    scroll3: set[int] = set()
    chunk_ids = {chunk for stage in stages["stages"] for chunk in stage["chunk_ids"] if chunk}
    for chunk in chunk_ids:
        start = 0x80000 + chunk * 0x400
        for cell in range(256):
            word = int.from_bytes(program[start + cell * 4 : start + cell * 4 + 2], "big")
            scroll2.add(word & 0x1FFF)
        start = 0xC0000 + chunk * 0x100
        for cell in range(64):
            word = int.from_bytes(program[start + cell * 4 : start + cell * 4 + 2], "big")
            # Scroll 3 is decoded as 32x32 tiles from the 4 MiB graphics
            # region. Codes beyond that physical region produce no ROM data.
            code = 0x0A00 + word
            if code < 0x2000:
                scroll3.add(code)
    return scroll2, scroll3


def referenced_chunks(stages: dict) -> set[int]:
    return {chunk for stage in stages["stages"] for chunk in stage["chunk_ids"] if chunk}


def pack_map_chunks(program: bytes, chunk_ids: set[int], base: int, stride: int) -> bytes:
    output = bytearray()
    for chunk in sorted(chunk_ids):
        start = base + chunk * stride
        output.extend(program[start : start + stride])
    return bytes(output)


def pack_tiles(gfx: bytearray, codes: set[int], size: int,
               reserve_blank: bool = False,
               sprite_transparency: bool = False,
               source_code_offset: int = 0, sprite_order: bool = False) -> bytes:
    output = bytearray(size * size // 2 if reserve_blank else 0)
    for code in sorted(codes):
        pixels = decode_tile(gfx, code + source_code_offset, size)
        if sprite_transparency:
            # CPS sprite pen 15 is transparent; Genesis sprite pen 0 is.
            # Rotate all source pens so artwork pen 0 remains opaque.
            pixels = [[(pen + 1) & 0x0F for pen in row] for row in pixels]
        packed = genesis_tiles(pixels)
        if sprite_order:
            packed = packed[:32] + packed[64:96] + packed[32:64] + packed[96:]
        output.extend(packed)
    return bytes(output)


def build_index_header(sprite_codes: set[int], proto_codes: set[int], scroll2_codes: set[int],
                       scroll3_codes: set[int], chunk_ids: set[int]) -> str:
    def mapping(size: int, codes: set[int], packed_offset: int = 0) -> list[int]:
        indices = [0xFFFF] * size
        for packed_index, code in enumerate(sorted(codes)):
            indices[code] = packed_index + packed_offset
        return indices

    rows = [
        "/* Generated sparse-to-packed tile indices; graphics remain uncompressed. */",
        "#ifndef WILLOW_STAGE_TILE_INDEX_H",
        "#define WILLOW_STAGE_TILE_INDEX_H",
        "#include <genesis.h>",
        f"#define WILLOW_STAGE_SPRITE_TILE_COUNT {len(sprite_codes)}",
        f"#define WILLOW_STAGE_SCROLL2_TILE_COUNT {len(scroll2_codes)}",
        f"#define WILLOW_STAGE_SCROLL3_TILE_COUNT {len(scroll3_codes)}",
        f"#define WILLOW_PROTO_SPRITE_TILE_COUNT {len(proto_codes)}",
    ]
    for name, size, codes, packed_offset in (
        ("sprite", 0x8000, sprite_codes, 1),
        ("scroll2", 0x2000, scroll2_codes, 0),
        ("scroll3", 0x1600, {code - 0x0A00 for code in scroll3_codes}, 0),
    ):
        values = mapping(size, codes, packed_offset)
        rows.append(f"static const u16 willow_stage_{name}_index[{size}] = {{")
        for offset in range(0, size, 16):
            rows.append("    " + ", ".join(f"0x{value:04x}" for value in values[offset:offset + 16]) + ",")
        rows.append("};")
    proto_values = mapping(0x8000, proto_codes)
    rows.append("static const u16 willow_proto_sprite_index[0x8000] = {")
    for offset in range(0, 0x8000, 16):
        rows.append("    " + ", ".join(f"0x{value:04x}" for value in proto_values[offset:offset + 16]) + ",")
    rows.append("};")
    rows.append("static const u16 willow_proto_sprite_codes[WILLOW_PROTO_SPRITE_TILE_COUNT] = {")
    ordered_proto = sorted(proto_codes)
    for offset in range(0, len(ordered_proto), 16):
        rows.append("    " + ", ".join(f"0x{value:04x}" for value in ordered_proto[offset:offset + 16]) + ",")
    rows.append("};")
    for name in ("scroll2", "scroll3"):
        values = mapping(256, chunk_ids)
        rows.append(f"static const u16 willow_stage_{name}_map_index[256] = {{")
        for offset in range(0, 256, 16):
            rows.append("    " + ", ".join(f"0x{value:04x}" for value in values[offset:offset + 16]) + ",")
        rows.append("};")
    rows.extend(["#endif", ""])
    return "\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output-dir", type=Path, default=Path("build"))
    parser.add_argument("--manifest", type=Path, default=Path("build/willow_stage_assets.json"))
    args = parser.parse_args()
    try:
        program = assemble_maincpu(args.archive)
        animations = build_catalog(program)
        stages = stage_catalog(program)
        with zipfile.ZipFile(args.archive) as zf:
            gfx = assemble_region(zf)
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        parser.error(str(exc))

    sprite_codes = referenced_sprite_codes(animations)
    proto_codes = prototype_sprite_codes(animations)
    scroll2_codes, scroll3_codes = referenced_layer_codes(program, stages)
    chunk_ids = referenced_chunks(stages)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    files = {
        "sprite_tiles": ("willow_stage_sprite_tiles.bin", pack_tiles(
            gfx, sprite_codes, 16, reserve_blank=True,
            sprite_transparency=True, sprite_order=True)),
        "scroll2_tiles": ("willow_stage_scroll2_tiles.bin", pack_tiles(
            gfx, scroll2_codes | {0}, 16, sprite_transparency=True, source_code_offset=0x4000)),
        "scroll3_tiles": ("willow_stage_scroll3_tiles.bin", pack_tiles(gfx, scroll3_codes, 32, sprite_transparency=True)),
        "scroll2_maps": ("willow_stage_scroll2_maps.bin", pack_map_chunks(program, chunk_ids, 0x80000, 0x400)),
        "scroll3_maps": ("willow_stage_scroll3_maps.bin", pack_map_chunks(program, chunk_ids, 0xC0000, 0x100)),
    }
    manifest = {"source": "decoded Willow graphics; complete 0x0000-0x7fff CPS graphics tile-code range plus stage-referenced layers",
                "encoding": "uncompressed Genesis 4bpp tile bytes, sorted by source CPS tile code",
                "indices_header": "willow_stage_tile_index.h",
                "assets": {}}
    for key, (filename, data) in files.items():
        (args.output_dir / filename).write_bytes(data)
        manifest["assets"][key] = {"file": filename, "bytes": len(data)}
    header = args.output_dir / "willow_stage_tile_index.h"
    header.write_text(build_index_header(sprite_codes, proto_codes, scroll2_codes | {0}, scroll3_codes, chunk_ids), encoding="ascii")
    manifest["assets"]["indices_header_bytes"] = header.stat().st_size
    manifest["sprite_source_codes"] = len(sprite_codes)
    manifest["prototype_sprite_source_codes"] = len(proto_codes)
    manifest["scroll2_source_codes"] = len(scroll2_codes | {0})
    manifest["scroll3_source_codes"] = len(scroll3_codes)
    manifest["referenced_map_chunks"] = len(chunk_ids)
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    print("Packed uncompressed native stage graphics:")
    for key, (_, data) in files.items():
        print(f"  {key}: {len(data):,} bytes")
    print(f"  sprite source codes: {len(sprite_codes):,}")
    print(f"  Scroll 2 source codes: {len(scroll2_codes | {0}):,}")
    print(f"  Scroll 3 source codes: {len(scroll3_codes):,}")
    print(f"  map chunks: {len(chunk_ids)} (ID zero is the blank fallback)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
