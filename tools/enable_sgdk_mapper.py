#!/usr/bin/env python3
"""Enable SGDK's official Sega mapper for ROMs larger than 4 MiB."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sgdk_root", type=Path)
    args = parser.parse_args()
    config = args.sgdk_root / "inc" / "config.h"
    source = config.read_text(encoding="utf-8")
    matches = list(re.finditer(
        r"(?m)^(\s*#define\s+ENABLE_BANK_SWITCH\s+)([01])(\s*(?://.*)?)$",
        source))
    if len(matches) != 1:
        parser.error(f"expected one ENABLE_BANK_SWITCH definition in {config}")
    match = matches[0]
    if match.group(2) == "0":
        source = source[:match.start(2)] + "1" + source[match.end(2):]
        config.write_text(source, encoding="utf-8")
    print(f"Enabled SGDK official mapper in {config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
