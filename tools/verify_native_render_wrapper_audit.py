#!/usr/bin/env python3
"""Read-only original producer-context and first-active-cache review."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "build/native-object-publication/checkpoint-compact-v1"
VISUAL = ROOT / "build/native-object-publication/checkpoint-visual-state-v1"
OUT = ROOT / "build/native-render-memory/wrapper-audit-v1"
SAME = ("frames.csv", "irqs.csv", "irq-ring.bin", "captured-original-banks.bin",
        "metadata-acquisitions.csv", "publication-latches.csv", "producer-epochs.csv",
        "headroom-events.csv", "headroom-windows.csv")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    baseline = json.loads((BASE / "evidence.json").read_text())
    require(sha(args.core) == baseline["core_sha256"], "Core changed")
    require(sha(ROOT / "build/reference_maincpu.bin") == baseline["source_rom_sha256"], "Original ROM changed")
    for directory in (BASE, VISUAL):
        for name, digest in json.loads((directory / "checkpoint.json").read_text())["files"].items():
            require(sha(directory / name) == digest, f"Changed pinned input: {name}")
    if not args.no_run:
        subprocess.run(["clang", "-Wall", "-Wextra", "-Werror", "-O2", "-fstrict-aliasing", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "tools/test_native_render_wrapper_audit.c", "-o", str(OUT / "test-wrapper-audit")], cwd=ROOT, check=True)
        with (OUT / "cadence.log").open("w") as log:
            subprocess.run([str(OUT / "test-wrapper-audit"), str(args.core), "1800", "--coin-route"], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    for name in SAME:
        require((OUT / name).read_bytes() == (BASE / name).read_bytes(), f"Observer changed {name}")
    producers = rows(OUT / "producer-contexts.csv")
    require(len(producers) == 1350, "Wrong producer count")
    for index, row in enumerate(producers, 1):
        require(int(row["epoch"]) == index and int(row["bank"]) in (0, 1), "Wrong producer identity")
        top, sp = int(row["task_top"], 16), int(row["sp"], 16)
        require(top - sp == 8 and int(row["free_bytes"]) == 120, "Unexpected source stack depth")
        require((row["current_task"], row["task_top"]) in (("20", "ff0310"), ("60", "ff0410")), "Unobserved task context")
    captures = rows(OUT / "publication-latches.csv")
    first = next(row for row in captures if row["manager"] == row["scene"] == "2")
    prior = producers[int(first["producer_epoch"]) - 1]
    require((first["frame"], first["selected_bank"], first["producer_epoch"], first["count"]) == ("1336", "0", "886", "3"), "First active bank changed")
    require((prior["frame"], prior["primary"], prior["manager"], prior["scene"], prior["demo"]) == ("1334", "2", "0", "24", "0"), "Pre-active producer changed")
    require(not any(row["frame"] == "1335" for row in producers), "Unexpected transition producer")
    visual = rows(VISUAL / "visual-contexts.csv")
    first_visual = next(row for row in visual if row["manager"] == row["scene"] == "2")
    preparation_states = [row for row in visual if row["manager"] == "0" and row["scene"] == "24"]
    with (VISUAL / "visual-contexts.bin").open("rb") as stream:
        stream.seek((int(first_visual["capture"]) - 1) * 90624)
        palette = stream.read(6144)
        require(len(palette) == 6144, "Incomplete palette record")
        for row in preparation_states:
            stream.seek((int(row["capture"]) - 1) * 90624)
            require(stream.read(6144) == palette, "Pre-active palette differs")
    require(len(preparation_states) == 66, "Wrong pre-active snapshot coverage")
    result = {
        "schema_version": 1,
        "scope": "Read-only original producer/source-stack and incoming first-active-bank observation; no new wrapper execution or live renderer admission.",
        "source_rom_sha256": baseline["source_rom_sha256"], "core_sha256": baseline["core_sha256"],
        "paired_frames": 1800, "producer_commits": len(producers),
        "minimum_source_free_bytes_at_displaced_publication": 120,
        "calculated_free_after_new_62_byte_save": 58,
        "stack_calculation_scope": "The added metadata JSR has returned before the save. This is observed source call depth plus arithmetic, not execution of the new wrapper or a global stack bound.",
        "first_active_acquisition": first, "its_producer_context": prior,
        "pre_active_palette_snapshots_equal": len(preparation_states),
        "palette_sha256": hashlib.sha256(palette).hexdigest(),
        "gate_finding": "Preparing only while manager/scene are 2/2 misses bank0 epoch886 used by the first two active acquisitions. Admit a supported producer through key/palette/view/epoch guards before the state change; publication can remain diagnostic active-only.",
        "source_transition": "Scene FF8008=0x18 dispatches through 9F84/9F90 to A41C. Pending text/timer branches A3E2->9EF2->3B524; A428 clears Scroll1, A432 increments manager to2, A436 sets scene2, and no producer runs in transition frame1335.",
        "identical_cadence_files": {name: sha(OUT / name) for name in SAME},
        "inputs": {str(p.relative_to(ROOT)): sha(p) for p in (BASE / "checkpoint.json", VISUAL / "checkpoint.json", ROOT / "src/willow_arcade_player_motion.h", ROOT / "tools/vendor/libretro.h")},
        "files": {name: sha(OUT / name) for name in ("producer-contexts.csv", "test-wrapper-audit", "cadence.log")},
        "tools": {name: sha(ROOT / name) for name in ("tools/test_native_render_wrapper_audit.c", "tools/verify_native_render_wrapper_audit.py")},
    }
    (OUT / "evidence.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("paired_frames", "producer_commits", "minimum_source_free_bytes_at_displaced_publication", "calculated_free_after_new_62_byte_save", "pre_active_palette_snapshots_equal")}, indent=2))


if __name__ == "__main__":
    main()
