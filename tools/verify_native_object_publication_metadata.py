#!/usr/bin/env python3
"""Validate target-executed source publication metadata, preserving full latch."""
import argparse
import csv
import hashlib
import json
import re
import statistics
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/native-object-publication/metadata-v1"
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return list(csv.DictReader(p.open()))
def stats(v): return {"samples": len(v), "min": min(v), "mean": statistics.mean(v), "max": max(v)}
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    parser.add_argument("--no-run", action="store_true")
    a = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    base = json.loads((ROOT / "build/native-cadence/checkpoint-latch-v2/evidence.json").read_text())
    assert sha(a.core) == base["core_sha256"]
    assert sha(ROOT / "build/reference_maincpu.bin") == base["source_rom_sha256"]
    if not a.no_run:
        subprocess.run(["python3", "tools/build_native_object_publication_metadata.py"], cwd=ROOT, check=True, stdout=(OUT / "build.log").open("w"))
        for source, output, args, log in (
            ("tools/test_native_object_publication_metadata.c", "test-metadata", ["1800", "--coin-route"], "cadence.log"),
            ("tools/test_native_object_publication_metadata_cost.c", "test-cost", [], "closed.log"),
        ):
            subprocess.run(["clang", "-Wall", "-Wextra", "-Werror", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", source, "-o", str(OUT / output)], cwd=ROOT, check=True)
            with (OUT / log).open("w") as f:
                subprocess.run([str(OUT / output), str(a.core), *args], cwd=ROOT, check=True, stdout=f, stderr=subprocess.STDOUT)
    manifest = json.loads((OUT / "manifest.json").read_text())
    for name, digest in manifest["products"].items(): assert sha(OUT / name) == digest
    for name, digest in manifest["source_files"].items(): assert sha(ROOT / name) == digest
    acquisitions = rows(OUT / "metadata-acquisitions.csv")
    source = rows(OUT / "publication-latches.csv")
    epochs = rows(OUT / "producer-epochs.csv")
    assert len(acquisitions) == len(source) == 1797 and len(epochs) == 1349
    for actual, expected in zip(acquisitions, source, strict=True):
        assert actual["frame"] == expected["frame"] and actual["capture"] == expected["capture"]
        assert actual["bank"] == expected["selected_bank"]
        assert actual["source_epoch"] == expected["producer_epoch"]
        assert actual["source_valid"] == expected["valid_complete_epoch"]
        if actual["native_valid"] == "1":
            assert actual["source_valid"] == "1"
            assert actual["native_epoch"] == actual["source_epoch"]
            assert actual["native_count"] == actual["source_count"]
    admitted = [r for r in acquisitions if r["native_valid"] == "1"]
    assert len(admitted) == 1606
    assert sum(a["native_valid"] != a["source_valid"] for a in acquisitions) == 0
    active_indices = [i for i, r in enumerate(source) if r["manager"] == r["scene"] == "2"]
    assert len(active_indices) == 463 and all(acquisitions[i]["native_valid"] == "1" for i in active_indices)
    irqs = rows(OUT / "irqs.csv")
    active_irqs = [r for r in irqs if r["manager"] == r["scene"] == "2"]
    missing = [r for r in active_irqs if not int(r["gameplay_wait_mask"], 16)]
    assert len(missing) == 1 and missing[0]["irq"] == "1335" and missing[0]["interrupted_pc"] == "4038a"
    log = (OUT / "cadence.log").read_text()
    assert "1800 paired physicalGenesis frames" in log
    assert "Native metadata task-stack observation: 16268 stores; minimum106 bytes" in log
    assert "4496 paired source/hook cases" in (OUT / "closed.log").read_text()
    costs = rows(OUT / "closed-cost.csv")
    assert len(costs) == 4496
    timing = {}
    for hook in dict.fromkeys(c["hook"] for c in costs):
        group = [c for c in costs if c["hook"] == hook]
        added = [int(c["added_master_cycles"]) for c in group]
        assert all(int(c["native_master_cycles"]) - int(c["source_master_cycles"]) == int(c["added_master_cycles"]) for c in group)
        assert {int(c["ccr"]) for c in group} == set(range(32))
        if hook != "acquire_and_latch": assert {int(c["supervisor"]) for c in group} == {0, 1}
        if hook in ("double_publish", "single_publish"): assert {int(c["remaining"]) for c in group} == set(range(257))
        timing[hook] = {"added_master_cycles": stats(added), "added_ms": stats([v / 53693.175 for v in added])}
    windows = rows(OUT / "headroom-windows.csv")
    waits = [w for w in windows if w["end_manager"] == w["end_scene"] == "2" and int(w["main_waits"])]
    result = {
        "schema_version": 1,
        "scope": "Native68000 per-bank publication metadata only. Full2048-byte latch retained. Independent original-address/native-relocated continuous source parity, not shipping or renderer/CPS-speed admission.",
        "source_rom_sha256": base["source_rom_sha256"], "core_sha256": base["core_sha256"],
        "native_metadata_text_bytes": len((OUT / "metadata.bin").read_bytes()),
        "metadata_storage": {"base": "20F500", "bytes": 48, "ownership": "Existing diagnostic reservation only, not production admission"},
        "paired_physical_frames": 1800, "source_irqs": 1797,
        "complete_source_producer_epochs": 1349,
        "native_admitted_captures": 1606, "native_rejected_fallbacks": 191,
        "native_extra_conservative_rejections": 0,
        "active_captures": 463, "active_native_admitted_captures": 463,
        "active_completed_gameplay_updates": 462,
        "transition_miss": {"irq": 1335, "frame": 1337, "interrupted_source_pc": "4038A",
                            "note": "Added hook cycles change the interrupted instruction versus frozen latch-v2's403E2. The missing transition update remains explicit."},
        "closed_hook_cases": 4496, "closed_hook_timing": timing,
        "closed_preservation": "Every displaced source instruction's register/CCR/USP/SSP effects exact in original user and supervisor modes; all non-call-stack RAM and nonmetadata SRAM compared. Exact temporary call-stack sizes14/18bytes, acquire wrapper12bytes; acquisition uses normal C caller-clobbers.",
        "task_stack_coverage": {"source_allocation_bytes": 128, "observed_native_hook_stores": 16268,
                                "minimum_free_bytes": 106, "scope": "This continuous route; not a global stack bound"},
        "source_scheduling": "No IRQ masking, invented waits, host target-register writes or source-task replacements. Both native continuations execute the same metadata hook overhead; timing remains Genesis-clock parity, not CPS10MHz equivalence.",
        "ready_protocol": "Begin invalidates bank before displaced source counter initialization. Commit follows exact original selector publication and sets ready last. IRQ acquisition reads prior hardware800100; a late ready flag may safely cause fallback. Unknown nonproducer object writer sites abort this fixture.",
        "retained_guards": "All source reads/writes into scattered native chunks still rejected; exact remaining/cursor/emitted-word/source-publication proof retained. Native faults reject admission.",
        "main_commit_to_next_irq_marker_upper_bound_ms": stats([(int(w["next_irq_master"])-int(w["main_commit_master"]))/53693.175 for w in waits]),
        "headroom_caution": "Includes remaining source tasks and hardware/native entry latency; not idle time or a guaranteed renderer budget.",
        "files": {n: sha(OUT / n) for n in ("manifest.json", "metadata-acquisitions.csv", "publication-latches.csv", "producer-epochs.csv", "object-write-sites.csv", "captured-original-banks.bin", "frames.csv", "irqs.csv", "irq-ring.bin", "pointer-sites.csv", "headroom-events.csv", "headroom-windows.csv", "cadence.log", "closed.log", "closed-cost.csv", "test-metadata", "test-cost")},
        "sources": {n: sha(ROOT / n) for n in ("tools/test_native_object_publication_metadata.c", "tools/test_native_object_publication_metadata_cost.c", "tools/verify_native_object_publication_metadata.py")}
    }
    (OUT / "evidence.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: result[k] for k in ("paired_physical_frames", "native_admitted_captures", "native_rejected_fallbacks", "closed_hook_cases", "closed_hook_timing", "main_commit_to_next_irq_marker_upper_bound_ms")}, indent=2))
if __name__ == "__main__": main()
