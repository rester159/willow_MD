#!/usr/bin/env python3
"""Catalog original command-12 HUD labels and their CPS Scroll1 writes.

4F54 queues command 12; 5BAA selects 5F6A via 5C7E's offset table.
The zero-delay renderer is instruction-verified by test_arcade_player_hud.c.
"""

from __future__ import annotations

import argparse
import json
import zipfile
from pathlib import Path

from cps1_program import assemble_maincpu


# Each record is x, y, palette/state byte, followed by the literal source text.
# '*' begins another positioned record; NUL terminates the entire stream.
RECORDS = (
    (0x5F6A, "1UP*", bytes.fromhex("0108003155502a")),
    (0x5F71, "HI*", bytes.fromhex("13080048492a")),
    (0x5F77, "2UP*", bytes.fromhex("2408003255502a")),
    (0x5F7E, "VITAL*", bytes.fromhex("010c00564954414c2a")),
    (0x5F87, "P=*", bytes.fromhex("296800503d2a")),
    (0x5F8D, "GOLD*", bytes.fromhex("130c02474f4c442a")),
    (0x5F95, "TIME*", bytes.fromhex("240c0354494d452a")),
    (0x5F9D, ":\0", bytes.fromhex("2a0c033a00")),
)
CHARACTER_GLYPH_ROUTINES = {
    "default": {"address": 0x5BEE, "base_code": 0x7000},
}
RENDERER_ADDRESS = 0x5BC8


def catalog(program: bytes) -> dict[str, object]:
    if 0x5c7e + int.from_bytes(program[0x5c96:0x5c98], "big", signed=True) != 0x5f6a:
        raise ValueError("Command-12 label pointer changed")
    result = []
    for address, text, expected in RECORDS:
        actual = program[address : address + len(expected)]
        if actual != expected:
            raise ValueError(
                f"HUD candidate record at 0x{address:04x} changed: "
                f"expected {expected.hex()}, got {actual.hex()}"
            )
        x, y, style = actual[:3]
        result.append({
            "address": f"0x{address:04x}",
            "raw": actual.hex(),
            "x": x,
            "y": y,
            "style": style,
                "text": text[:-1],
                "terminator": "next_record" if text[-1] == "*" else "end_stream",
                "destination": f"0x{0x910000 + x * 128 + y:06x}",
                "glyph_codes_default": [f"0x{0x7000 + ord(char):04x}" for char in text[:-1]],
                "context": "4F54 command 12; 5BAA/5BC8 renderer; x is column, y is byte offset",
        })
    return {
        "source": "offline Willow USA main-CPU image",
        "renderer_address": f"0x{RENDERER_ADDRESS:04x}",
        "glyph_code_routines": {
            key: {"address": f"0x{value['address']:04x}",
                  "base_code": f"0x{value['base_code']:04x}"}
            for key, value in CHARACTER_GLYPH_ROUTINES.items()
        },
        "records": result,
        "limitations": [
            "This catalog covers label data; task scheduling and native graphics conversion are separate.",
            "Dynamic health, gold, time, and score writes are translated in willow_arcade_player_hud.h.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_hud_candidates.json"))
    args = parser.parse_args()
    try:
        report = catalog(assemble_maincpu(args.archive))
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(report['records'])} byte-validated HUD candidate records to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
