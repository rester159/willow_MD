#!/usr/bin/env python3
"""Checked CPS port-address mirror for source-native IRQ/input experiments.

The mirror does not implement CPS latch side effects or a Genesis audio/input
driver. Its caller supplies raw active-low input bytes and must consume ordered
object/palette/sound publication events. Keep this separate from shipping code.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from native_video_relocation import ROOT, SOURCE_SHA256, relocate as relocate_video

PORT_SOURCE = 0x800000
PORT_BYTES = 0x200
DEFAULT_MIRROR = 0x20F800

# Each tuple identifies the address operand, original address, and all bytes
# from the containing instruction through the byte before that address. This
# distinguishes actual absolute MOVE operands from data/immediate lookalikes.
PORT_OPERANDS = (
    (0x408, 0x800030, "13fc0080"),
    (0x410, 0x800030, "13fc0000"),
    (0x420, 0x800181, "13fc00f0"),
    (0x428, 0x80010C, "33fcffc0"),
    (0x430, 0x80010E, "33fc0000"),
    (0x438, 0x800100, "33fc9000"),
    (0x440, 0x800102, "33fc9100"),
    (0x448, 0x800104, "33fc9180"),
    (0x450, 0x800106, "33fc9140"),
    (0x478, 0x800170, "33fc12e0"),
    (0x480, 0x800166, "33fc003f"),
    (0x488, 0x80010A, "33fc91c0"),
    (0x490, 0x800122, "33fc003e"),
    (0x5BE, 0x800122, "33fc003e"),
    (0x5DE, 0x800100, "33fc9000"),
    (0x5EC, 0x800102, "33fc9100"),
    (0x5FA, 0x800104, "33fc9180"),
    (0x608, 0x800106, "33fc9140"),
    (0x616, 0x800108, "33fc9220"),
    (0x155C, 0x800100, "33ed0022"),
    (0x15BE, 0x800166, "33ed0054"),
    (0x15C6, 0x80010A, "33ed002c"),
    (0x18E6, 0x800122, "33c0"),
    (0x18EE, 0x800170, "33ed004a"),
    (0x18F6, 0x800030, "13ed0296"),
    (0x18FE, 0x80016E, "33ed004c"),
    (0x1906, 0x80016C, "33ed004e"),
    (0x190E, 0x80016A, "33ed0050"),
    (0x1916, 0x800168, "33ed0052"),
    (0x1926, 0x800018, "1039"),
    (0x193C, 0x800019, "1039"),
    (0x1958, 0x800001, "1039"),
    (0x1964, 0x800000, "1039"),
    (0x1970, 0x800011, "1039"),
    (0x1982, 0x80010C, "33ed7442"),
    (0x1994, 0x800110, "33ed7446"),
    (0x19A6, 0x800114, "33ed744a"),
    (0x19B8, 0x800120, "33ed0042"),
    (0x19C0, 0x800118, "33ed744e"),
    (0x19DE, 0x80011C, "33ed7452"),
    (0x19FC, 0x80010E, "33ed7444"),
    (0x1A10, 0x800112, "33ed7448"),
    (0x1A24, 0x800116, "33ed744c"),
    (0x1A38, 0x80011A, "33ed7450"),
    (0x1A52, 0x80011E, "33ed7454"),
    (0x1A6A, 0x80001A, "1039"),
    (0x1A76, 0x80001C, "1039"),
    (0x1A82, 0x80001E, "1039"),
    (0x7800, 0x800189, "13f40000"),
    (0x7808, 0x800181, "13f40001"),
)


def relocate(source: bytes, mirror: int = DEFAULT_MIRROR,
             with_video: bool = False) -> tuple[bytes, dict]:
    if len(source) != 0x100000 or hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError("source program size/SHA256 mismatch")
    # The simultaneous-video bridge reserves F000..F7FF for state, and its
    # caller stack/canaries reserve FE00..FFFF. Do not silently occupy either.
    if mirror & 1 or not 0x20F800 <= mirror <= 0x20FE00 - PORT_BYTES:
        raise ValueError("experimental port mirror must be aligned within 20F800..20FDFF")
    initial, video = relocate_video(source) if with_video else (source, None)
    result = bytearray(initial)
    operands = []
    seen = set()
    for offset, address, prefix in PORT_OPERANDS:
        expected = bytes.fromhex(prefix) + address.to_bytes(4, "big")
        start = offset - len(bytes.fromhex(prefix))
        if source[start:offset + 4] != expected:
            raise ValueError(f"source port instruction changed at {start:x}")
        if offset in seen or not PORT_SOURCE <= address < PORT_SOURCE + PORT_BYTES:
            raise ValueError("invalid/duplicate port operand")
        seen.add(offset)
        target = mirror + address - PORT_SOURCE
        result[offset:offset + 4] = target.to_bytes(4, "big")
        operands.append({"instruction": start, "offset": offset, "size": 4,
                         "source": address, "target": target,
                         "instruction_prefix": prefix})
    output = bytes(result)
    return output, {
        "scope": "experimental port mirror; not an implemented hardware adapter or shipping cartridge",
        "source_sha256": SOURCE_SHA256,
        "output_sha256": hashlib.sha256(output).hexdigest(),
        "mirror": mirror, "mirror_bytes": PORT_BYTES,
        "port_operand_count": len(operands), "port_operands": operands,
        "video_relocation": video,
        "required_side_effects": [
            "800100 object publication must preserve the source write's ordering",
            "80010A palette publication must preserve the source write's ordering",
            "800189 then 800181 sound writes are ordered events, even for repeated values",
            "800000/1,800011,800018/19,80001A/1C/1E are source raw input/DIP bytes",
            "Genesis VBlank/IRQ cadence, display and sound remain unimplemented here",
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "build/reference_maincpu.bin")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--mirror", type=lambda value: int(value, 0), default=DEFAULT_MIRROR)
    parser.add_argument("--with-video", action="store_true")
    args = parser.parse_args()
    paths = [path.resolve() for path in (args.source, args.output, args.manifest)]
    if len(set(paths)) != 3:
        parser.error("source, output and manifest must have distinct paths")
    output, manifest = relocate(args.source.read_bytes(), args.mirror, args.with_video)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"{manifest['port_operand_count']} checked port operands -> {args.output}")


if __name__ == "__main__":
    main()
