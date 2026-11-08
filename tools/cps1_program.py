#!/usr/bin/env python3
"""Rebuild Willow's CPS-1 main CPU region for offline disassembly only.

The resulting reference binary is analysis material. It is not a resource and
is never linked into the native SGDK cartridge image.
"""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path

REGION_SIZE = 0x100000


def assemble_maincpu(archive: Path) -> bytearray:
    """Apply Willow USA's ROM_LOAD16_BYTE and ROM_LOAD16_WORD_SWAP mappings."""
    rom = bytearray(REGION_SIZE)
    with zipfile.ZipFile(archive) as zf:
        for even_name, odd_name, base in (
            ("wlu_30.11f", "35.11h", 0x00000),
            ("wlu_31.12f", "wlu_36.12h", 0x40000),
        ):
            even, odd = zf.read(even_name), zf.read(odd_name)
            if len(even) != 0x20000 or len(odd) != 0x20000:
                raise ValueError(f"Unexpected program ROM size: {even_name}/{odd_name}")
            rom[base : base + 0x40000 : 2] = even
            rom[base + 1 : base + 0x40000 : 2] = odd

        word_swapped = zf.read("wlm-32.8h")
        if len(word_swapped) != 0x80000:
            raise ValueError("Unexpected program ROM size: wlm-32.8h")
        for offset in range(0, len(word_swapped), 2):
            rom[0x80000 + offset : 0x80002 + offset] = word_swapped[offset : offset + 2][::-1]
    return rom


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/reference_maincpu.bin"))
    args = parser.parse_args()
    try:
        data = assemble_maincpu(args.archive)
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"Wrote {len(data):,}-byte offline main CPU reference to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
