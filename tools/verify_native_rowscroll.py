#!/usr/bin/env python3
"""Reproduce the isolated, source-checked row-scroll relocation diagnostic."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA = "0b66efe85aec1e94fcaf722de9401cc7e4ec09647daf5f9f0232116861240cc1"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    args = parser.parse_args()
    source = ROOT / "build/reference_maincpu.bin"
    if source.stat().st_size != 0x100000 or digest(source) != SOURCE_SHA:
        raise ValueError("original program size/SHA mismatch")
    core = args.core.resolve(strict=True)
    out = ROOT / "build/native-rowscroll"
    out.mkdir(exist_ok=True)
    commands = [
        ["clang", "-Wall", "-Wextra", "-Werror", "-O2", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "tools/test_native_rowscroll.c", "-o", str(out / "test")],
        [str(out / "test"), str(core)],
    ]
    subprocess.run(commands[0], cwd=ROOT, check=True)
    with (out / "full.log").open("w") as log:
        subprocess.run(commands[1], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    paths = [source, core, ROOT / "tools/test_native_rowscroll.c", Path(__file__).resolve(), ROOT / "src/willow_arcade_player_motion.h", ROOT / "tools/vendor/libretro.h", out / "test", out / "closed-oracle.bin", out / "full.log"]
    evidence = {
        "scope": "closed original690C routine; no production layout or rendering admission",
        "source_region": {"start": 0x922000, "size": 0x800},
        "candidate_region": {"start": 0x20C800, "size": 0x800, "restriction": "test-only; replaces displayed object latch"},
        "patch": {"instruction": 0x6980, "opcode": "207c", "operand": 0x6982, "old": 0x922000, "new": 0x20C800},
        "cases": 8192,
        "negative_initializations": 4096,
        "source_word_writes": 4194304,
        "commands": commands,
        "files": {str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path): digest(path) for path in paths},
    }
    (out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print((out / "full.log").read_text(), end="")


if __name__ == "__main__":
    main()
