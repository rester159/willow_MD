#!/usr/bin/env python3
"""Inventory Willow's supplied MAME ROM set without executing arcade code."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path


def inspect(archive: Path) -> dict[str, object]:
    result: dict[str, object] = {"archive": archive.name, "entries": []}
    with zipfile.ZipFile(archive) as zf:
        for info in sorted(zf.infolist(), key=lambda item: item.filename.lower()):
            if info.is_dir():
                continue
            data = zf.read(info)
            result["entries"].append(
                {
                    "name": info.filename,
                    "size": len(data),
                    "zip_crc32": f"{info.CRC:08x}",
                    "sha1": hashlib.sha1(data).hexdigest(),
                }
            )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", nargs="?", type=Path, default=Path("willow.zip"))
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()
    try:
        report = inspect(args.archive)
    except (OSError, zipfile.BadZipFile) as exc:
        print(f"ROM audit failed: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        entries = report["entries"]
        total = sum(row["size"] for row in entries)
        print(f"Archive: {report['archive']} ({len(entries)} files, {total:,} bytes)")
        for row in entries:
            print(f"{row['name']:<16} {row['size']:>8,}  CRC32 {row['zip_crc32']}  SHA1 {row['sha1']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
