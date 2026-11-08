#!/usr/bin/env python3
"""Verify read-only pre-IRQ visual-state captures against pinned compact cadence."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/native-object-publication/compact-visual-state-v1"
BASE = ROOT / "build/native-object-publication/checkpoint-compact-v1"
STRIDE = 90624


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    return list(csv.DictReader(path.open()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    base = json.loads((BASE / "evidence.json").read_text())
    assert sha(args.core) == base["core_sha256"]
    assert sha(ROOT / "build/reference_maincpu.bin") == base["source_rom_sha256"]
    products = json.loads((BASE / "manifest.json").read_text())["products"]
    for name, digest in products.items():
        assert sha(ROOT / "build/native-object-publication/compact-v1" / name) == digest
    if not args.no_run:
        subprocess.run(["clang", "-Wall", "-Wextra", "-Werror", "-O2", "-fstrict-aliasing", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "tools/test_native_compact_visual_state.c", "-o", str(OUT / "test-visual")], cwd=ROOT, check=True)
        with (OUT / "cadence.log").open("w") as log:
            subprocess.run([str(OUT / "test-visual"), str(args.core), "1800", "--coin-route"], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    identical = ("frames.csv", "irqs.csv", "irq-ring.bin", "captured-original-banks.bin", "metadata-acquisitions.csv", "publication-latches.csv", "producer-epochs.csv", "headroom-events.csv", "headroom-windows.csv")
    for name in identical:
        assert (OUT / name).read_bytes() == (BASE / name).read_bytes(), name
    index = rows(OUT / "visual-contexts.csv")
    captures = rows(OUT / "publication-latches.csv")
    metadata = rows(OUT / "metadata-acquisitions.csv")
    data = (OUT / "visual-contexts.bin").read_bytes()
    assert len(index) == len(captures) == len(metadata) == 1797
    assert len(data) == len(index) * STRIDE
    for i, (row, capture, native) in enumerate(zip(index, captures, metadata, strict=True)):
        assert row["capture"] == capture["capture"] == native["capture"] == str(i + 1)
        for key in ("frame", "manager", "scene", "master_cycles"):
            assert row[key] == capture[key]
        assert row["bank"] == capture["selected_bank"] == native["bank"]
        assert row["source_epoch"] == capture["producer_epoch"] == native["source_epoch"]
        for key in ("native_epoch", "native_count", "native_valid", "source_count", "source_valid"):
            assert row[key] == native[key]
        at = i * STRIDE
        assert int(row["prior_selector"], 16) == struct.unpack_from(">H", data, at + 6144 + 0x100)[0]
        assert int(row["next_selector"], 16) == struct.unpack_from(">H", data, at + 8704 + 0x8022)[0]
        assert int(row["manager"]) == struct.unpack_from(">H", data, at + 8704 + 0x8004)[0]
        assert int(row["scene"]) == struct.unpack_from(">H", data, at + 8704 + 0x8008)[0]
    # Match the earlier independent narrow transition probe, including all RAM.
    transition = BASE / "transition"
    old_index = rows(transition / "transition-contexts.csv")
    old = (transition / "transition-contexts.bin").read_bytes()
    for i, row in enumerate(old_index):
        at = (int(row["capture"]) - 1) * STRIDE
        record = data[at:at + STRIDE]
        assert record[:6656] + record[8704:74240] == old[i * 72192:(i + 1) * 72192]
    sites = rows(OUT / "scroll1-sites.csv")
    reads = rows(OUT / "scroll1-reads.csv")
    writes = rows(OUT / "scroll1-writes.csv")
    assert len(sites) == 74 and not reads and len(writes) == 86238
    assert sum(int(row["count"]) for row in sites) == len(writes)
    layout = {
        "schema_version": 1, "byte_order": "big-endian source address order", "records": len(index),
        "record_bytes": STRIDE, "index": "visual-contexts.csv", "object_banks": "captured-original-banks.bin", "object_bank_bytes": 2048,
        "boundary": "Original path pre-source-1554 object acquisition, after target native metadata acquisition, using prior 800100 selector. Read-only host peeks do not change target or cycles. Candidate uses unchanged bus and full frame-end memory/register/cycle parity; no separate same-instruction candidate snapshot claim.",
        "regions": [
            {"name": "palettes", "offset": 0, "size": 6144, "source_start": "91C000"},
            {"name": "ports", "offset": 6144, "size": 512, "source_start": "800000"},
            {"name": "row_scroll", "offset": 6656, "size": 2048, "source_start": "922000"},
            {"name": "work_ram", "offset": 8704, "size": 65536, "source_start": "FF0000"},
            {"name": "scroll1", "offset": 74240, "size": 16384, "source_start": "910000"},
        ],
    }
    (OUT / "layout.json").write_text(json.dumps(layout, indent=2) + "\n")
    result = {
        "schema_version": 1, "scope": "Read-only exact pre-IRQ original-source visual state on the compact continuous Genesis-clock route; no rendering, color fitting, full-game or CPS-speed admission.",
        "source_rom_sha256": base["source_rom_sha256"], "core_sha256": base["core_sha256"],
        "target_products": products, "paired_frames": 1800, "captures": 1797,
        "active_captures": sum(row["manager"] == row["scene"] == "2" for row in index),
        "identical_cadence_files": {name: sha(OUT / name) for name in identical},
        "previous_transition_snapshots_identical": len(old_index),
        "scroll1_word_writes": len(writes), "scroll1_write_sites": len(sites), "observed_scroll1_readbacks": 0,
        "readback_scope": "This route starts at6F2 and skips power-on memory diagnostics. Zero observed reads is not a whole-ROM write-only proof.",
        "new_files": {name: sha(OUT / name) for name in ("visual-contexts.bin", "visual-contexts.csv", "layout.json", "scroll1-sites.csv", "scroll1-reads.csv", "scroll1-writes.csv", "test-visual", "cadence.log")},
        "sources": {name: sha(ROOT / name) for name in ("tools/test_native_compact_visual_state.c", "tools/verify_native_compact_visual_state.py")},
    }
    (OUT / "evidence.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ("paired_frames", "captures", "active_captures", "previous_transition_snapshots_identical", "scroll1_word_writes", "scroll1_write_sites", "observed_scroll1_readbacks")}, indent=2))


if __name__ == "__main__":
    main()
