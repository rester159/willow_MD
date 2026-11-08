#!/usr/bin/env python3
"""Guarded simultaneous CPS-video address relocation for the native experiment.

This writes a program image, not a bootable cartridge. The enumerated operands
and tables come from the existing original-instruction region fixtures. It does
not scan and rewrite arbitrary ROM values or mutate retained RAM pointers.
Full simultaneous execution, the platform IRQ and the mapper still need proof.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1"


@dataclass(frozen=True)
class Region:
    name: str
    source: int
    target: int
    size: int


REGIONS = (
    Region("scroll1", 0x910000, 0x200000, 0x4000),
    Region("scroll2", 0x918000, 0x204000, 0x4000),
    Region("scroll3", 0x914000, 0x208000, 0x4000),
    Region("object0", 0x900000, 0x20C000, 0x0800),
    Region("object1", 0x908000, 0x20D000, 0x0800),
    Region("palettes", 0x91C000, 0x20D800, 0x1800),
)
DISPLAYED_OBJECTS = (0x20C800, 0x0800)
AUXILIARY = (0x20F000, 0x1000)
OBJECT_BANK_BIT = 12


@dataclass(frozen=True)
class Patch:
    offset: int
    size: int
    source: int
    target: int
    purpose: str


def mapped_address(address: int) -> int:
    for region in REGIONS:
        if region.source <= address < region.source + region.size:
            return region.target + address - region.source
    raise ValueError(f"not a mapped CPS video address: {address:08x}")


def validate_layout() -> None:
    spans = [(r.target, r.size, r.name) for r in REGIONS]
    spans += [(*DISPLAYED_OBJECTS, "displayed_objects"), (*AUXILIARY, "auxiliary")]
    end = 0x200000
    for start, size, name in sorted(spans):
        if start != end:
            raise ValueError(f"layout overlap/gap before {name}: {end:x}/{start:x}")
        end = start + size
    if end != 0x210000:
        raise ValueError("layout must occupy exactly 64 KiB")
    for region in REGIONS:
        if region.name.startswith("scroll") and region.target & 0x3FFF:
            raise ValueError("plane alignment would change source address arithmetic")
        if region.name.startswith("object"):
            for cursor in range(region.size + 1):
                if ((region.source + cursor) >> 15 & 1) != (
                    (region.target + cursor) >> OBJECT_BANK_BIT & 1
                ):
                    raise ValueError("object bank discriminator fails at cursor boundary")


def patches_for(source: bytes) -> list[Patch]:
    if len(source) != 0x100000 or hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError("source program size/SHA256 mismatch")
    validate_layout()
    patches: list[Patch] = []

    def add(offset: int, expected: int, purpose: str) -> None:
        actual = int.from_bytes(source[offset:offset + 4], "big")
        if actual != expected:
            raise ValueError(f"operand {offset:x}: {actual:x} != {expected:x}")
        patches.append(Patch(offset, 4, actual, mapped_address(actual), purpose))

    for region, clear, layer_clear in zip(
        REGIONS[:5], (0x5876, 0x588A, 0x589E, 0x58BC, 0x58C6),
        (0x5A14, 0x5A50, 0x5A90, 0x59D8, 0x59E2), strict=True
    ):
        add(clear, region.source, f"clear {region.name}")
        add(layer_clear, region.source, f"layer clear {region.name}")
    for region, offset in zip(REGIONS[:3], (0x6644, 0x6656, 0x6668), strict=True):
        add(offset, region.source, f"reset {region.name}")
    for region, offsets in zip(
        REGIONS[3:5], ((0x6620, 0x3B538, 0x58F8), (0x662A, 0x3B54C, 0x5902)), strict=True
    ):
        for offset in offsets:
            add(offset, region.source, f"object writer/reset {region.name}")
    for index, offset in enumerate((0x51EE, 0x51FE, 0x520E, 0x521E, 0x522E, 0x523E)):
        add(offset, 0x91C000 + index * 1024, "fade palette page")
    for index, offset in enumerate((0x531C, 0x5346, 0x5370, 0x539A, 0x53C2, 0x53F0, 0x541E, 0x544C)):
        add(offset, 0x91C000 + index % 4 * 1024, "bank palette fade")
    for offset, address in (
        (0xB38, 0x91C800), (0xB4A, 0x91CC00),
        (0xAE4, 0x91C800), (0xB06, 0x91CC00),
        (0xAF6, 0x91C400), (0xB16, 0x91C200),
        (0x988, 0x91C000), (0x99A, 0x91C200), (0x9B0, 0x91C800),
        (0x9CC, 0x91C400), (0x9DA, 0x91CC00), (0xC242, 0x91C000),
        (0xEA68, 0x91C0E0), (0xC2E, 0x91C000), (0x1276, 0x91C000),
    ):
        add(offset, address, "stage/message palette")
    for offset, address in (
        (0x32E5A, 0x9197DC), (0x31FBA, 0x918000),
        (0x4365A, 0x918000), (0x43D50, 0x914000), (0x3E44, 0x918000),
    ):
        add(offset, address, "stage scroll/collision pointer")
    for index in range(12):
        offset = 0x340B6 + index * 4
        address = int.from_bytes(source[offset:offset + 4], "big")
        if not 0x918000 <= address < 0x91C000:
            raise ValueError("stage pointer table left Scroll2")
        add(offset, address, "stage Scroll2 pointer table")
    text_offsets = (
        0x679E, 0x7D74, 0x7D84, 0x5BD0, 0x4F56, 0x4F82, 0x4F1A, 0x4F30,
        0x4D6C, 0x4DA8, 0x4FC6, 0x5012, 0x504C, 0x5062, 0x4C50, 0x4A36,
        0x4A40, 0x56CA, 0x6308, 0x6316, 0x6320, 0x632E, 0x633C, 0x4E34,
        0x4E4A, 0x4E5E, 0x5080, 0x5084, 0x5088, 0x508C, 0x5090, 0x5094,
        0x5098, 0x5C32, 0x4B72,
    )
    text_addresses = (
        0x910034, 0x910000, 0x910200, 0x910000, 0x910070, 0x9100F0,
        0x91140C, 0x91158C, 0x910DF0, 0x910DF0, 0x910170, 0x910A70,
        0x910AF0, 0x910D70, 0x910B70, 0x91030C, 0x91030C, 0x9115E8,
        0x911388, 0x910208, 0x910208, 0x911388, 0x910A88, 0x910C0C,
        0x910D0C, 0x910E0C, 0x910A70, 0x910BF0, 0x910CF0, 0x910D70,
        0x910D70, 0x910D70, 0x910D70, 0x910000, 0x910170,
    )
    for offset, address in zip(text_offsets, text_addresses, strict=True):
        add(offset, address, "HUD/message Scroll1 pointer")
    table_offsets = [0x7A1E + index * 4 for index in range(47)]
    table_offsets += [base + index * 4 for base in (0x7C50, 0x7C64) for index in range(5)]
    for offset in table_offsets:
        address = int.from_bytes(source[offset:offset + 4], "big")
        if not 0x910000 <= address < 0x914000:
            raise ValueError("message pointer table left Scroll1")
        add(offset, address, "HUD/message Scroll1 pointer table")
    # BDB4 indexes these ten longwords with MOVEA.L (PC,D1.W),A0;
    # BDBC writes attributes through the resulting Scroll1 pointer.
    if source[0xBDB4:0xBDB8] != bytes.fromhex("207b1016"):
        raise ValueError("message pointer-table consumer changed")
    for offset in range(0xBDCC, 0xBDF4, 4):
        address = int.from_bytes(source[offset:offset + 4], "big")
        if not 0x910000 <= address < 0x914000:
            raise ValueError("message attribute table left Scroll1")
        add(offset, address, "BDB4 message attribute pointer table")
    catalogue = json.loads((ROOT / "tools/native_video_operands.json").read_text())
    if catalogue["source_sha256"] != SOURCE_SHA256:
        raise ValueError("address-load catalogue source mismatch")
    for entry in catalogue["operands"]:
        offset = int(entry["offset"], 16)
        opcode = int(entry["instruction_word"], 16)
        if opcode & 0xF1FF not in (0x41F9, 0x207C):
            raise ValueError("catalogue is restricted to LEA/MOVEA address loads")
        if int.from_bytes(source[offset - 2:offset], "big") != opcode:
            raise ValueError(f"address-load opcode changed at {offset - 2:x}")
        add(offset, int(entry["source"], 16), entry["kind"])
    # MOVE.L A0,D0; BTST #15,D0. Keep the instruction/flags semantics, and
    # change only the bit number proven over both entire cursor intervals.
    if source[0x3B57A:0x3B580] != bytes.fromhex("20080800000f"):
        raise ValueError("source object discriminator instructions changed")
    patches.append(Patch(0x3B57E, 2, 15, OBJECT_BANK_BIT, "post-write object bank discriminator"))
    patches.sort(key=lambda item: item.offset)
    end = 0
    for patch in patches:
        if patch.offset < end:
            raise ValueError("overlapping relocation operands")
        end = patch.offset + patch.size
    return patches


def relocate(source: bytes) -> tuple[bytes, dict]:
    patches = patches_for(source)
    target = bytearray(source)
    for patch in patches:
        target[patch.offset:patch.offset + patch.size] = patch.target.to_bytes(patch.size, "big")
    result = bytes(target)
    evidence = {
        "scope": "candidate program relocation; not a bootable or validated production cartridge",
        "source_sha256": SOURCE_SHA256,
        "output_sha256": hashlib.sha256(result).hexdigest(),
        "hardware_requirement": "64 KiB byte-addressable 16-bit SRAM, both byte lanes",
        "regions": [asdict(region) for region in REGIONS],
        "displayed_objects": {"target": DISPLAYED_OBJECTS[0], "size": DISPLAYED_OBJECTS[1]},
        "auxiliary": {"target": AUXILIARY[0], "size": AUXILIARY[1]},
        "patch_count": len(patches),
        "operand_catalogue_sha256": hashlib.sha256(
            (ROOT / "tools/native_video_operands.json").read_bytes()).hexdigest(),
        "patches": [asdict(patch) for patch in patches],
        "unverified": ["all-region execution and retained-pointer parity", "hardware ports and IRQ",
                       "production SRAM mapper", "rendering/audio and complete-frame timing"],
    }
    return result, evidence


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "build/reference_maincpu.bin")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    if args.source.resolve() in (args.output.resolve(), args.manifest.resolve()):
        parser.error("do not overwrite the source ROM")
    if args.output.resolve() == args.manifest.resolve():
        parser.error("output and manifest paths must differ")
    target, evidence = relocate(args.source.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(target)
    args.manifest.write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"{len(evidence['patches'])} checked video operands -> {args.output}")


if __name__ == "__main__":
    main()
