#!/usr/bin/env python3
"""Catalog Willow's compact CPS Scroll 1 map streams for native conversion.

This reads only the offline 68000 reference image. It does not execute arcade
code or include that code in the Genesis build.
"""

from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

from cps1_program import assemble_maincpu

TABLE_BASE = 0x5C7E
TABLE_ENTRIES = 32
ROM_LIMIT = 0x100000
MAP_BYTES = 0x4000
MAP_WIDTH = 64
MAP_HEIGHT = 64
MAP_COLUMN_STRIDE = 32
STREAM_LIMIT = 0x4000
SCROLL1_STREAM_CODES = 0x5B20
SCROLL1_STREAM_CODES_BYTES = bytes.fromhex(
    "e308654441fa01583030000041f0000070007400220243f900910000"
    "1418ef4a1218d44143f1200012187400141867160c02002a67dc0642"
    "700032823341000243e9008060e44e75"
)
SCROLL1_STREAM_FILL = 0x5B68
SCROLL1_STREAM_FILL_BYTES = bytes.fromhex(
    "41fa01143030000041f0000070007400220243f9009100001418ef4a"
    "1218d44143f120001218343c7020101867120c00002a67da32823341"
    "000243e9008060ea4e75"
)
MAP_0A_0B_CONTEXT = 0x8B26
MAP_0A_0B_BYTES = bytes.fromhex(
    "082d000002af67f6546d0008"
    "303c008a4eb85b2061000132610000b2"
    "303c000b4eb85b20"
)
MAP_CLEAR_HELPER_BYTES = bytes.fromhex(
    "207c00911164303c7020323c00096124207c00911168323c00096118"
    "207c0091116c323c0009610c207c00911170323c00094e71"
    "3080317c0000000241e8008051c9fff24e75"
)
MAP_TEMPLATE_HELPER_BYTES = bytes.fromhex(
    "343c0014207c0091116c303c7180323c000a6154"
    "207c00911170303c7190323c000a6044"
)
MAP_TEMPLATE_WRITER_BYTES = bytes.fromhex(
    "3080314200020640000141e8008051c9fff04e75"
)
OBJECT_SCROLL1_PALETTE_OVERLAY = 0x425A6
OBJECT_SCROLL1_PALETTE_OVERLAY_BYTES = bytes.fromhex(
    "0a2e0001004c4a2e004c66046000f0b47000267c00080000"
    "302e004ae148e588d7c0207c00910000303c000f343c000f"
    "7200322b0002ee490241000330810658700030fc000047eb"
    "000441e8000451caffe041e8008051c8ffd44e75"
)
OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT = 0x424C8
OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT_BYTES = bytes.fromhex(
    "08050000660000d8"
)
WILLOW_DSWC_MODE_LATCH = 0x1A80
WILLOW_DSWC_MODE_LATCH_BYTES = bytes.fromhex(
    "10390080001e46001b4000684e75102d006802400010e7481b400295"
    "422d799e102d006802400080ee481b40028f671061000060082d000300"
    "686704522d799e"
)
WILLOW_TEST_MODE_DISPATCH = 0x80A6
WILLOW_TEST_MODE_DISPATCH_BYTES = bytes.fromhex(
    "4a2d028f6612303c0020207c000080d04eb815cc4ef81636303c0080"
    "207c00045a8c4eb815cc"
)
WILLOW_TEST_MODE_ENTRY = 0x45A8C
WILLOW_TEST_MODE_ENTRY_BYTES = bytes.fromhex(
    "1b7c000102a11b7c00010293422d0294422d0292422d02a5422d799c"
    "4eb8784c610000de426d03444eb809783b7c90000022082d0004006067"
    "0a4eb82de64ef9000415c2"
)


def be16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "big")


def cell_coordinates(tile_index: int) -> tuple[int, int]:
    column = (tile_index >> 5) & 0x3F
    row = (tile_index & 0x1F) | ((tile_index >> 6) & 0x20)
    return column, row


def parse_stream(data: bytes, start: int, mode: str) -> dict[str, object]:
    """Decode stream headers and per-column tile writes used by 0x5b20."""
    if mode not in ("codes", "fill"):
        raise ValueError(f"unknown stream mode: {mode}")
    cursor = start
    cells: list[dict[str, int]] = []
    segments_data: list[list[dict[str, int]]] = []
    segments = 0
    terminator = "stream limit"

    for _ in range(STREAM_LIMIT):
        if cursor + 2 > min(len(data), ROM_LIMIT):
            terminator = "outside ROM"
            break
        column_byte, row_byte = data[cursor : cursor + 2]
        cursor += 2
        destination = (column_byte << 7) + row_byte
        if destination >= MAP_BYTES or row_byte & 3:
            terminator = "invalid map address"
            break
        segments += 1
        segment_cells: list[dict[str, int]] = []
        tile_index = destination >> 2

        if mode == "codes":
            if cursor >= min(len(data), ROM_LIMIT):
                terminator = "outside ROM"
                break
            attribute = data[cursor]
            cursor += 1
            for _ in range(STREAM_LIMIT):
                if cursor >= min(len(data), ROM_LIMIT):
                    terminator = "outside ROM"
                    break
                code = data[cursor]
                cursor += 1
                if code == 0:
                    terminator = "end marker"
                    break
                if code == 0x2A:
                    break
                column, row = cell_coordinates(tile_index)
                cell = {
                    "column": column,
                    "row": row,
                    "tile_index": tile_index,
                    "cps_code": 0x7000 + code,
                    "attribute": attribute,
                    "palette": attribute & 0x1F,
                    "flip_x": (attribute >> 5) & 1,
                    "flip_y": (attribute >> 6) & 1,
                    "layout": (tile_index >> 5) & 1,
                }
                cells.append(cell)
                segment_cells.append(cell)
                tile_index += MAP_COLUMN_STRIDE
            else:
                terminator = "pair limit"
        else:
            if cursor >= min(len(data), ROM_LIMIT):
                terminator = "outside ROM"
                break
            attribute = data[cursor]
            cursor += 1
            for _ in range(STREAM_LIMIT):
                if cursor >= min(len(data), ROM_LIMIT):
                    terminator = "outside ROM"
                    break
                count = data[cursor]
                cursor += 1
                if count == 0:
                    terminator = "end marker"
                    break
                if count == 0x2A:
                    break
                column, row = cell_coordinates(tile_index)
                cell = {
                    "column": column,
                    "row": row,
                    "tile_index": tile_index,
                    "cps_code": 0x7020,
                    "attribute": attribute,
                    "palette": attribute & 0x1F,
                    "flip_x": (attribute >> 5) & 1,
                    "flip_y": (attribute >> 6) & 1,
                    "layout": (tile_index >> 5) & 1,
                }
                cells.append(cell)
                segment_cells.append(cell)
                tile_index += MAP_COLUMN_STRIDE
            else:
                terminator = "run limit"

        segments_data.append(segment_cells)
        if terminator == "end marker":
            break
        if cursor + 2 > min(len(data), ROM_LIMIT):
            terminator = "outside ROM"
            break
    else:
        terminator = "segment limit"

    return {
        "mode": mode,
        "segments": segments,
        "segment_cells": segments_data,
        "cells": cells,
        "termination": terminator,
        "bytes_consumed": cursor - start,
    }


def find_map_selectors(data: bytes) -> dict[int, list[dict[str, object]]]:
    """Find immediate selector values passed to direct calls of 0x5b20."""
    references: dict[int, list[dict[str, object]]] = {}
    for site in range(0, len(data) - 8, 2):
        if data[site : site + 2] != b"\x30\x3c":  # move.w #imm,d0
            continue
        selector = be16(data, site + 2)
        call = site + 4
        target = None
        if data[call : call + 4] == b"\x4e\xb8\x5b\x20":  # jsr $5b20
            target = 0x5B20
        elif data[call : call + 2] == b"\x61\x00":  # bsr.w $5b20
            displacement = int.from_bytes(data[call + 2 : call + 4], "big", signed=True)
            target = call + 2 + displacement
        if target != 0x5B20:
            continue
        index = selector & 0x7F
        references.setdefault(index, []).append({
            "call_site": f"0x{site + 4:06x}",
            "selector": f"0x{selector:04x}",
            "mode": "fill" if selector & 0x80 else "codes",
        })
    return references


def build_catalog(data: bytes) -> dict[str, object]:
    maps: list[dict[str, object]] = []
    selectors = find_map_selectors(data)
    if data[SCROLL1_STREAM_CODES:
            SCROLL1_STREAM_CODES + len(SCROLL1_STREAM_CODES_BYTES)] \
            != SCROLL1_STREAM_CODES_BYTES:
        raise ValueError("Scroll 1 code-run writer at 0x5b20 changed")
    if data[SCROLL1_STREAM_FILL:
            SCROLL1_STREAM_FILL + len(SCROLL1_STREAM_FILL_BYTES)] \
            != SCROLL1_STREAM_FILL_BYTES:
        raise ValueError("Scroll 1 fill-run writer at 0x5b68 changed")
    if data[MAP_0A_0B_CONTEXT:MAP_0A_0B_CONTEXT + len(MAP_0A_0B_BYTES)] != MAP_0A_0B_BYTES:
        raise ValueError("conditional Scroll 1 calls for maps 0x0a/0x0b changed")
    if data[0x8C6E:0x8C6E + len(MAP_CLEAR_HELPER_BYTES)] != MAP_CLEAR_HELPER_BYTES:
        raise ValueError("Scroll 1 tile clear helper at 0x8c6e changed")
    if data[0x8BF2:0x8BF2 + len(MAP_TEMPLATE_HELPER_BYTES)] != MAP_TEMPLATE_HELPER_BYTES:
        raise ValueError("Scroll 1 tile template helper at 0x8bf2 changed")
    if data[0x8C5A:0x8C5A + len(MAP_TEMPLATE_WRITER_BYTES)] != MAP_TEMPLATE_WRITER_BYTES:
        raise ValueError("Scroll 1 tile template writer at 0x8c5a changed")
    overlay_end = OBJECT_SCROLL1_PALETTE_OVERLAY + len(OBJECT_SCROLL1_PALETTE_OVERLAY_BYTES)
    if data[OBJECT_SCROLL1_PALETTE_OVERLAY:overlay_end] != OBJECT_SCROLL1_PALETTE_OVERLAY_BYTES:
        raise ValueError("object Scroll 1 palette overlay at 0x425a6 changed")
    overlay_input_end = OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT + len(OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT_BYTES)
    if data[OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT:overlay_input_end] != OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT_BYTES:
        raise ValueError("Scroll 1 overlay input branch at 0x424c8 changed")
    if data[WILLOW_DSWC_MODE_LATCH:WILLOW_DSWC_MODE_LATCH + len(WILLOW_DSWC_MODE_LATCH_BYTES)] != WILLOW_DSWC_MODE_LATCH_BYTES:
        raise ValueError("Willow DSWC mode sampler at 0x1a80 changed")
    if data[WILLOW_TEST_MODE_DISPATCH:WILLOW_TEST_MODE_DISPATCH + len(WILLOW_TEST_MODE_DISPATCH_BYTES)] != WILLOW_TEST_MODE_DISPATCH_BYTES:
        raise ValueError("Willow Test-mode dispatcher at 0x80a6 changed")
    if data[WILLOW_TEST_MODE_ENTRY:WILLOW_TEST_MODE_ENTRY + len(WILLOW_TEST_MODE_ENTRY_BYTES)] != WILLOW_TEST_MODE_ENTRY_BYTES:
        raise ValueError("Willow Test-mode entry at 0x45a8c changed")
    conditional_overlay = {
        "routine_entry": "0x008b26",
        "guard": "returns at 0x008b24 when bit 0 of a5+0x02af is clear; maps load only when it is set",
        "post_guard": "increments word a5+0x0008 by 2 before loading maps",
        "map_0a_call": "0x008b36 (fill mode)",
        "intervening_calls": [
            {"site": "0x008b3a", "target": "0x008c6e"},
            {"site": "0x008b3e", "target": "0x008bf2"},
        ],
        "tile_writes": {
            "0x008c6e": "writes code 0x7020 and attribute 0 to ten successive rows in each map column at 0x91164, 0x91168, 0x9116c, and 0x91170",
            "0x008bf2": "writes code runs 0x7180/0x7190 (ten rows) and 0x71c0/0x71d0/0x71e0/0x71f0 (nine rows) in columns at 0x91164-0x91170; attributes are 0x14 then 0x13",
            "writer": "0x008c5a stores code at a0 and attribute at a0+2, increments code, then advances a0 by 0x80 bytes per row",
        },
        "map_0b_call": "0x008b46 (codes mode)",
        "order": ["0x0a", "0x8c6e", "0x8bf2", "0x0b"],
        "gameplay_role": "unresolved; do not assume this is the ordinary gameplay HUD",
    }
    for index in range(TABLE_ENTRIES):
        relative = be16(data, TABLE_BASE + index * 2)
        start = TABLE_BASE + relative
        if start >= ROM_LIMIT:
            status = "outside ROM"
            modes: dict[str, object] = {}
        elif data[start : start + 2] == b"\x4e\x75":
            status = "empty return stub"
            modes = {}
        elif ((data[start + 1] & 3) != 0
              or (((data[start] << 7) + data[start + 1]) >= MAP_BYTES)):
            # Entries 23..31 share offset 0x055e, which points into the
            # following program routine rather than to a tilemap stream.
            status = "not a map stream"
            modes = {}
        else:
            status = "candidate map stream"
            modes = {
                "codes": parse_stream(data, start, "codes"),
                "fill": parse_stream(data, start, "fill"),
            }
        runtime_context = None
        if index in (10, 11):
            runtime_context = conditional_overlay
        elif index == 13 and any(
            call["call_site"] == "0x00565e"
            for call in selectors.get(index, [])
        ):
            runtime_context = {
                "containing_routine": "0x005630",
                "caller_site": "0x03807c",
                "guard": "a5+0x028f != 0 and bit 5 of a5+0x0068 is set",
                "flag_source": "a5+0x0068 is the inverted CPS DSWC byte; a5+0x028f copies its bit 7",
                "dip_meaning": "DSWC bit 7 selects Test mode; bit 5 enables Display Debug Infos",
                "source_reference": "https://github.com/mamedev/mame/blob/master/src/mame/capcom/cps1.cpp",
                "selector": "0x000d (codes mode)",
                "note": "debug-info overlay selected in Test mode; exact displayed values and palette mapping remain unresolved",
            }
        maps.append({
            "index": index,
            "table_word": f"0x{TABLE_BASE + index * 2:06x}",
            "relative_offset": f"0x{relative:04x}",
            "stream_start": f"0x{start:06x}",
            "status": status,
            "selector_calls": selectors.get(index, []),
            "runtime_context": runtime_context,
            "ascii_text_candidate": ascii_text_candidate(
                modes.get("codes", {"cells": []})),
            "modes": modes,
        })
    return {
        "source": "offline Willow USA 68000 region",
        "routine": "0x005b20",
        "stream_writers": {
            "codes": {"offset": "0x005b20", "bytes": SCROLL1_STREAM_CODES_BYTES.hex(),
                      "format": "one attribute byte, then glyph-code bytes"},
            "fill": {"offset": "0x005b68", "bytes": SCROLL1_STREAM_FILL_BYTES.hex(),
                     "format": "one attribute byte, then fill-run bytes"},
        },
        "table": f"0x{TABLE_BASE:06x}",
        "table_entry_count": TABLE_ENTRIES,
        "tilemap": {"width": MAP_WIDTH, "height": MAP_HEIGHT, "entry_bytes": 4},
        "format_notes": {
            "address": "first byte is column, second byte is a byte offset within the 32-row half",
            "codes_mode": "one attribute byte followed by code bytes; code 0 ends, code 0x2a starts another segment",
            "fill_mode": "one attribute followed by fill-run bytes; 0 ends, 0x2a starts another segment",
            "layout": "CPS Scroll 1 8x8 layout selected by tilemap memory-index bit 5 (alternating logical columns)",
        },
        "object_scroll1_palette_overlay": {
            "entry": f"0x{OBJECT_SCROLL1_PALETTE_OVERLAY:06x}",
            "instruction_bytes": OBJECT_SCROLL1_PALETTE_OVERLAY_BYTES.hex(),
            "input_branch": {
                "site": f"0x{OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT:06x}",
                "instruction_bytes": OBJECT_SCROLL1_PALETTE_OVERLAY_INPUT_BYTES.hex(),
                "condition": "newly pressed bit 0 from the input mask assembled at 0x424b6",
                "target": f"0x{OBJECT_SCROLL1_PALETTE_OVERLAY:06x}",
            },
            "object_fields": {
                "+74": "selects a 16x16-cell block from the Scroll 2 map at 0x800000",
                "+76": "toggles between rebuilding the overlay and restoring default Scroll 1/Scroll 3 maps",
            },
            "set_path": "reads each Scroll 2 cell palette group from attribute bits 7-8 and writes Scroll 1 code 0x7000+group with attribute zero to a 16x16 region at 0x910000",
            "clear_path": "branches to 0x41668, which restores default Scroll 1 and Scroll 3 map entries",
            "dispatch_context": "the DSWC sampler at 0x1a80 inverts 0x80001e and copies bit 7 into a5+0x28f; the dispatcher at 0x80a6 routes that flag to the Test-mode entry at 0x45a8c, which initializes state and transfers to 0x415c2; the interactive input handler at 0x424b2 is within this diagnostic flow",
            "mode_evidence": {
                "sampler": f"0x{WILLOW_DSWC_MODE_LATCH:06x}",
                "dispatcher": f"0x{WILLOW_TEST_MODE_DISPATCH:06x}",
                "entry": f"0x{WILLOW_TEST_MODE_ENTRY:06x}",
                "mame_meaning": "Willow DSWC bit 7 selects Test mode (MAME cps1.cpp Willow DIP definition)",
                "confidence": "byte-validated mode latch and branch path; external DIP meaning is documented by MAME",
            },
            "gameplay_use": "Test-mode diagnostic/map-inspection UI, not an ordinary Stage 0 actor or layer; preserve as a separate native service/debug state if reproducing Test mode, and never add this overlay to the persistent stage renderer",
        },
        "maps": maps,
    }


def ascii_text_candidate(parsed: dict[str, object]) -> str | None:
    """Expose printable code runs without asserting who displays the map."""
    segments = parsed.get("segment_cells", [parsed.get("cells", [])])
    codes = [[int(cell["cps_code"]) & 0xFF for cell in segment]
             for segment in segments]
    codes = [segment for segment in codes if segment]
    if not codes or any(code < 0x20 or code > 0x7E
                        for segment in codes for code in segment):
        return None
    return "\n".join("".join(chr(code) for code in segment)
                     for segment in codes)


def frontend_header(catalog: dict[str, object]) -> str:
    """Emit the source-verified Start prompt constants for the native runtime."""
    maps = catalog["maps"]
    prompt = maps[6]
    cells = prompt["modes"]["codes"]["cells"]
    text = prompt["ascii_text_candidate"]
    if (prompt["status"] != "candidate map stream" or text != "PRESS START BUTTON"
            or len(cells) != len(text)):
        raise ValueError("Scroll 1 map 6 no longer matches the Start prompt")
    x = int(cells[0]["column"])
    y = int(cells[0]["row"])
    palette = int(cells[0]["palette"])
    if any(int(cell["column"]) != x + index
           or int(cell["row"]) != y
           or int(cell["palette"]) != palette
           for index, cell in enumerate(cells)):
        raise ValueError("Scroll 1 map 6 Start prompt layout changed")
    return (
        "/* Generated from the Willow Scroll 1 map catalog. */\n"
        "#ifndef WILLOW_FRONTEND_H\n#define WILLOW_FRONTEND_H\n"
        f'#define WILLOW_START_PROMPT "{text}"\n'
        f"#define WILLOW_START_PROMPT_LENGTH {len(text)}\n"
        f"#define WILLOW_START_PROMPT_X {x}\n"
        f"#define WILLOW_START_PROMPT_Y {y}\n"
        f"#define WILLOW_START_PROMPT_PALETTE {palette}\n"
        "#endif\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_scroll1_maps.json"))
    parser.add_argument("--header", type=Path)
    args = parser.parse_args()
    try:
        data = bytes(assemble_maincpu(args.archive))
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        parser.error(str(exc))
    catalog = build_catalog(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    if args.header:
        args.header.parent.mkdir(parents=True, exist_ok=True)
        args.header.write_text(frontend_header(catalog), encoding="utf-8")
    valid = sum(row["status"] == "candidate map stream" for row in catalog["maps"])
    print(f"Cataloged {valid} Willow Scroll 1 map streams to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
