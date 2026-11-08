#!/usr/bin/env python3
"""Check that a finalized native Willow ROM does not contain arcade CPU ROMs.

This is a packaging guard, not a proof of behavioral accuracy. It checks the
assembled offline main-CPU region and every source CPU ROM, including the Z80
sound program, for verbatim inclusion in the Genesis image.
"""

from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path

from cps1_program import assemble_maincpu
from finalize_genesis_rom import DELIVERY_MAX_BYTES


CPU_ROM_NAMES = ("wlu_30.11f", "35.11h", "wlu_31.12f", "wlu_36.12h", "wlm-32.8h")
SOUND_CPU_ROM_NAME = "wl_09.12b"
MAPPER_CPU_WINDOW = 0x00400000
MAPPER_MAX_IMAGE_SIZE = 64 * 0x80000  # 64 switchable 512 KiB banks (32 MiB).
ROM_SIZE_ALIGNMENT = 0x20000
MAPPER_BANK_SIZE = 0x80000
PCM_MAPPER_WINDOW_START = 0x00200000
PCM_MAPPER_WINDOW_END = 0x00300000


def audit(archive: Path, rom_path: Path, max_bytes: int = DELIVERY_MAX_BYTES) -> list[str]:
    image = rom_path.read_bytes()
    errors: list[str] = []
    if len(image) > max_bytes:
        errors.append(f"cartridge is {len(image):,} bytes; delivery limit is {max_bytes:,} bytes")
    region = assemble_maincpu(archive)
    if bytes(region) in image:
        errors.append("assembled 1 MiB arcade main-CPU region is embedded verbatim")
    with zipfile.ZipFile(archive) as zf:
        for name in (*CPU_ROM_NAMES, SOUND_CPU_ROM_NAME):
            payload = zf.read(name)
            if payload in image:
                errors.append(f"arcade CPU ROM {name} is embedded verbatim")
    if len(image) < 0x200:
        errors.append("image is too small to contain a Genesis ROM header")
    elif image[0x100:0x110] not in (
            b"SEGA GENESIS    ", b"SEGA MEGA DRIVE ", b"SEGA SSF        "):
        errors.append("Genesis console field is missing or malformed")
    else:
        console = image[0x100:0x110]
        end_address = int.from_bytes(image[0x1A4:0x1A8], "big")
        if len(image) % ROM_SIZE_ALIGNMENT:
            errors.append("ROM size must be padded to a 128 KiB boundary")
        if console != b"SEGA SSF        ":
            if len(image) > MAPPER_CPU_WINDOW:
                errors.append("ROM above 4 MiB requires the banked Sega SSF header")
            if end_address != len(image) - 1:
                errors.append("unbanked ROM end address does not match image size")
        checksum_end = min(len(image), end_address + 1)
        actual_checksum = sum(
            int.from_bytes(image[offset : offset + 2], "big")
            for offset in range(0x200, checksum_end, 2)
        ) & 0xFFFF
        expected_checksum = int.from_bytes(image[0x18E:0x190], "big")
        if actual_checksum != expected_checksum:
            errors.append(
                f"Genesis checksum mismatch: header 0x{expected_checksum:04X}, "
                f"calculated 0x{actual_checksum:04X}"
            )
        if console == b"SEGA SSF        ":
            if end_address != 0x003FFFFF:
                errors.append("Sega mapper ROM must advertise its 4 MiB CPU window")
            # The 4 MiB address window is distinct from physical capacity.
            # All physical banks count toward DELIVERY_MAX_BYTES above.
            if len(image) > MAPPER_MAX_IMAGE_SIZE:
                errors.append("Sega mapper ROM exceeds the official mapper's 32 MiB limit")
    return errors


def audit_pcm_symbols(symbol_path: Path, audio_index_path: Path,
                      audio_catalog_path: Path) -> list[str]:
    """Audit the shared 16 kHz PCM4 sample block and its mapper windows."""
    errors: list[str] = []
    try:
        audio_index = json.loads(audio_index_path.read_text())
        audio_catalog = json.loads(audio_catalog_path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return [f"PCM source manifests could not be read: {exc}"]
    valid_sample_ids = {
        int(entry["id"]) for entry in audio_index["samples"]
        if "file" in entry
    }
    routed_sample_ids = {
        int(entry["sample_id"], 16)
        for entry in audio_catalog["commands"]
        if int(entry["sample_id"], 16) in valid_sample_ids
    }
    if valid_sample_ids != routed_sample_ids:
        missing = sorted(valid_sample_ids - routed_sample_ids)
        errors.append(f"valid OKI sample IDs lack direct commands: {missing}")
    indexed_sample_ids = {
        int(entry["sample_id"]) for entry in
        audio_index["pcm4_pcm_samples"]
    }
    if indexed_sample_ids != valid_sample_ids:
        errors.append("16 kHz PCM4 manifest does not cover every valid OKI sample")
    symbols: dict[str, tuple[int, str]] = {}
    for line in symbol_path.read_text().splitlines():
        fields = line.split()
        if len(fields) == 3:
            try:
                symbols[fields[2]] = (int(fields[0], 16), fields[1])
            except ValueError:
                continue

    # The packed signed 8-bit sample bank must occupy exactly two adjacent
    # mapper banks. Each manifest entry is checked against the packed offsets.
    block_entry = symbols.get("willow_pcm4_block")
    size_entry = symbols.get("willow_pcm4_block_size")
    if block_entry is None or size_entry is None:
        errors.append("packed PCM4 resource symbols are missing")
        block_start = block_size = 0
    else:
        block_start, block_type = block_entry
        block_size, size_type = size_entry
        if block_type not in ("T", "D", "R") or size_type != "A":
            errors.append("packed PCM4 resource symbols have unexpected types")
        if block_start & 0xFFFFF:
            errors.append("PCM4 sample block is not aligned to a 1 MiB boundary")
        if block_size != 0x100000:
            errors.append("PCM4 sample block is not exactly two complete mapper banks")
        if block_start + block_size > MAPPER_MAX_IMAGE_SIZE:
            errors.append("PCM4 sample block exceeds the mapper ROM limit")
        if block_start >> 19 >= 0x40 or (block_start >> 19) + 1 >= 0x40:
            errors.append("PCM4 sample bank pair exceeds the mapper bank limit")

    for entry in audio_index["pcm4_pcm_samples"]:
        sample_id = int(entry["sample_id"])
        offset = int(entry["offset"])
        size = int(entry["size"])
        padded_size = int(entry["padded_size"])
        if sample_id not in valid_sample_ids:
            errors.append(f"PCM4 manifest contains invalid sample {sample_id:#x}")
        if offset & 0xFF or padded_size & 0xFF or size <= 0:
            errors.append(f"PCM4 sample {sample_id:#x} has invalid alignment/size")
        expected_padded_size = (size + 0xFF) & ~0xFF
        if padded_size != expected_padded_size:
            errors.append(
                f"PCM4 sample {sample_id:#x} padding does not round its length "
                "up to the driver's 256-byte unit"
            )
        if size > padded_size or offset + padded_size > block_size:
            errors.append(f"PCM4 sample {sample_id:#x} exceeds the packed bank")

    # Sample 0x19 is the only direct OKI cue with nonzero attenuation. Its
    # command path must use a separately half-gain PCM4 clip so the native
    # high-rate route does not quantize that gain to a driver volume step.
    attenuated_variants = audio_index.get(
        "pcm4_pcm_attenuated_variants", [])
    if len(attenuated_variants) != 1:
        errors.append("PCM4 manifest must contain the sample 0x19 volume-2 variant")
    for entry in attenuated_variants:
        sample_id = int(entry["sample_id"])
        volume_nibble = int(entry["volume_nibble"])
        offset = int(entry["offset"])
        size = int(entry["size"])
        padded_size = int(entry["padded_size"])
        if sample_id != 0x19 or volume_nibble != 2:
            errors.append("unexpected PCM4 attenuated sample variant")
        if offset & 0xFF or padded_size & 0xFF or size <= 0:
            errors.append("PCM4 attenuated variant has invalid alignment/size")
        if padded_size != ((size + 0xFF) & ~0xFF):
            errors.append("PCM4 attenuated variant padding is not 256-byte aligned")
        if offset + padded_size > block_size:
            errors.append("PCM4 attenuated variant exceeds the packed bank")
        primary_offsets = [
            (int(sample["offset"]), int(sample["padded_size"]))
            for sample in audio_index["pcm4_pcm_samples"]
        ]
        if any(offset < start + length and start < offset + padded_size
               for start, length in primary_offsets):
            errors.append("PCM4 attenuated variant overlaps a primary sample")

    attenuated_commands = [
        entry for entry in audio_catalog["commands"]
        if int(entry["volume_nibble"], 16) != 0
        and int(entry["sample_id"], 16) in valid_sample_ids
    ]
    expected_pcm4_variants = {
        (int(entry["sample_id"], 16), int(entry["volume_nibble"], 16))
        for entry in attenuated_commands
    }
    actual_pcm4_variants = {
        (int(entry["sample_id"]), int(entry["volume_nibble"]))
        for entry in attenuated_variants
    }
    if actual_pcm4_variants != expected_pcm4_variants:
        errors.append(
            "PCM4 attenuated variants do not match the source command table")
    for sample_id in sorted({int(entry["sample_id"], 16)
                              for entry in attenuated_commands}):
        name = f"oki_sample_{sample_id:02x}_vol2"
        if name not in symbols:
            errors.append(f"attenuated XGM resource symbol is missing for {name}")
    for name, (address, symbol_type) in symbols.items():
        if (PCM_MAPPER_WINDOW_START <= address < PCM_MAPPER_WINDOW_END
                and symbol_type in ("T", "D", "R")
                and name != "willow_pcm4_block"):
            errors.append(
                f"non-PCM4 symbol {name} occupies reserved PCM mapper windows"
            )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="finalized Genesis ROM image")
    parser.add_argument("--archive", type=Path, default=Path("willow.zip"))
    parser.add_argument("--max-bytes", type=int, default=DELIVERY_MAX_BYTES,
                        help="total image budget; larger values are research only")
    parser.add_argument("--symbols", type=Path,
                        help="SGDK symbol table for checking direct PCM assets")
    parser.add_argument("--audio-index", type=Path,
                        default=Path("build/oki_wav/index.json"))
    parser.add_argument("--audio-catalog", type=Path,
                        default=Path("build/willow_audio_commands.json"))
    args = parser.parse_args()
    try:
        errors = audit(args.archive, args.rom, args.max_bytes)
        if args.symbols:
            errors.extend(audit_pcm_symbols(args.symbols, args.audio_index,
                                             args.audio_catalog))
    except (OSError, zipfile.BadZipFile, KeyError, ValueError) as exc:
        print(f"Native ROM audit failed: {exc}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"Native ROM audit passed: {args.rom.stat().st_size:,}/{args.max_bytes:,} bytes; "
          f"no verbatim arcade CPU ROM data in {args.rom}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
