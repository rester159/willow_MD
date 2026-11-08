#!/usr/bin/env python3
"""Set the Genesis ROM end address and checksum on the final padded image."""

from __future__ import annotations

import argparse
from pathlib import Path

HEADER_END_ADDRESS = 0x1A4
HEADER_CHECKSUM = 0x18E
HEADER_SIZE = 0x200
ROM_SIZE_ALIGNMENT = 131072
MAPPER_HEADER = b"SEGA SSF        "
MAPPER_WINDOW_END = 0x003FFFFF
DELIVERY_MAX_BYTES = 5 * 1024 * 1024


def finalize(path: Path, max_bytes: int = DELIVERY_MAX_BYTES) -> tuple[int, int]:
    image = bytearray(path.read_bytes())
    if len(image) <= HEADER_SIZE:
        raise ValueError("ROM must be larger than its header")
    padded_size = (len(image) + ROM_SIZE_ALIGNMENT - 1) & ~(ROM_SIZE_ALIGNMENT - 1)
    if max_bytes <= HEADER_SIZE or padded_size > max_bytes:
        raise ValueError(
            f"padded ROM needs {padded_size:,} bytes; cartridge limit is {max_bytes:,} bytes"
        )
    image.extend(b"\x00" * (padded_size - len(image)))
    if len(image) <= HEADER_SIZE or len(image) & 1:
        raise ValueError("ROM must be larger than its header and have an even byte length")
    console = bytes(image[0x100:0x110])
    if console == MAPPER_HEADER:
        # SSF uses the official Sega mapper to expose physical ROM banks
        # through a 4 MiB CPU address window. The header describes that
        # visible window; checksum only covers the fixed logical image.
        end_address = MAPPER_WINDOW_END
    elif console in (b"SEGA MEGA DRIVE ", b"SEGA GENESIS    "):
        if len(image) > MAPPER_WINDOW_END + 1:
            raise ValueError("ROM above 4 MiB requires the banked Sega SSF header")
        end_address = len(image) - 1
    else:
        raise ValueError("expected a supported Sega ROM header at 0x100")

    image[HEADER_END_ADDRESS : HEADER_END_ADDRESS + 4] = end_address.to_bytes(4, "big")
    checksum_end = min(len(image), end_address + 1)
    checksum = sum(
        int.from_bytes(image[offset : offset + 2], "big")
        for offset in range(HEADER_SIZE, checksum_end, 2)
    ) & 0xFFFF
    image[HEADER_CHECKSUM : HEADER_CHECKSUM + 2] = checksum.to_bytes(2, "big")
    path.write_bytes(image)
    return end_address, checksum


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--max-bytes", type=int, default=DELIVERY_MAX_BYTES,
                        help="total padded cartridge budget; larger values are research only")
    args = parser.parse_args()
    try:
        end_address, checksum = finalize(args.rom, args.max_bytes)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"ROM finalization failed: {exc}\n")
    print(f"ROM header finalized: bytes={args.rom.stat().st_size}, "
          f"limit={args.max_bytes}, end=0x{end_address:06X}, checksum=0x{checksum:04X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
