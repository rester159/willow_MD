#!/usr/bin/env python3
"""Catalog the fixed-size CPS layer-map chunks read by Willow's map viewer.

The dump is decoded offline. The generated catalog is analysis data only and
is never linked into the Genesis ROM.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from cps1_program import assemble_maincpu


def catalog(region: bytes) -> dict:
    layers = []
    for name, base, stride, word_count, cell_count, tile_bias, code_mask in (
        ("scroll2", 0x80000, 0x400, 512, 256, 0, 0x1FFF),
        ("scroll3", 0xC0000, 0x100, 128, 64, 0x0A00, 0xFFFF),
    ):
        maps = []
        for index in range(256):
            start = base + index * stride
            raw = region[start : start + stride]
            words = [int.from_bytes(raw[pos : pos + 2], "big")
                     for pos in range(0, word_count * 2, 2)]
            codes = [((words[pos] & code_mask) + tile_bias)
                     for pos in range(0, word_count, 2)]
            attributes = words[1::2]
            maps.append({
                "index": index,
                "source_offset": f"0x{start:06x}",
                "nonzero_words": sum(word != 0 for word in words),
                "word_count": word_count,
                "cell_count": cell_count,
                "words_per_cell": 2,
                "tile_bias": f"0x{tile_bias:04x}",
                "code_mask": f"0x{code_mask:04x}",
                "tile_codes_min": min(codes),
                "tile_codes_max": max(codes),
                "tile_codes": [f"0x{code:04x}" for code in codes],
                "attributes": [f"0x{word:04x}" for word in attributes],
            })
        layers.append({"layer": name, "source_base": f"0x{base:06x}",
                       "stride": f"0x{stride:03x}", "maps": maps})
    return {
        "source": "offline Willow 68000 ROM data",
        "usage": "CPS map viewer chunks; gameplay stage use not established",
        "copy_sites": {
            "scroll2": {"routine": "0x041ed6", "destination": "0x918000",
                         "bytes_per_selection": 1024},
            "scroll3": {"routine": "0x04447c", "destination": "0x914000",
                         "bytes_per_selection": 256},
        },
        "layers": layers,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/willow_layer_maps.json"))
    args = parser.parse_args()
    try:
        data = catalog(assemble_maincpu(args.archive))
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + "\n")
    for layer in data["layers"]:
        active = sum(m["nonzero_words"] > 0 for m in layer["maps"])
        print(f"Catalogued {active}/256 nonempty {layer['layer']} chunks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
