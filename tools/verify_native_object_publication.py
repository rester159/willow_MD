#!/usr/bin/env python3
"""Prove source-produced object epochs/counts and selected-prefix fidelity."""
import argparse
import csv
import hashlib
import json
import statistics
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "build/native-cadence/checkpoint-latch-v2"
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p): return list(csv.DictReader(p.open()))
def stats(v): return {"min": min(v), "mean": statistics.mean(v), "max": max(v)}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, default=ROOT / "build/native-object-publication")
    p.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    p.add_argument("--no-run", action="store_true")
    a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    pinned = json.loads((BASE / "evidence.json").read_text())
    assert sha(ROOT / "build/reference_maincpu.bin") == pinned["source_rom_sha256"]
    assert sha(a.core) == pinned["core_sha256"]
    for n in ("program.bin", "platform.bin", "platform-original.bin", "platform-addresses.h"):
        assert sha(BASE / n) == pinned["files"][n]
    source = ROOT / "tools/test_native_object_publication.c"
    if not a.no_run:
        subprocess.run(["clang", "-Wall", "-Wextra", "-Werror", "-O1", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", str(source), "-o", str(out / "test-publication")], check=True, cwd=ROOT)
        with (out / "cadence.log").open("w") as log:
            subprocess.run([str(out / "test-publication"), str(a.core), "1800", "--coin-route", str(out)], cwd=ROOT, check=True, stdout=log, stderr=subprocess.STDOUT)
    for n in ("frames.csv", "irqs.csv", "irq-ring.bin", "pointer-sites.csv", "oracle.bin"):
        assert (BASE / n).read_bytes() == (out / n).read_bytes(), n
    captures = rows(out / "publication-latches.csv")
    epochs = rows(out / "producer-epochs.csv")
    sites = rows(out / "object-write-sites.csv")
    assert len(captures) == 1797 and len(epochs) == 1349
    valid = [c for c in captures if c["valid_complete_epoch"] == "1"]
    active = [c for c in captures if c["manager"] == c["scene"] == "2"]
    assert len(valid) == 1606 and len(active) == 463
    assert all(c["valid_complete_epoch"] == "1" for c in active)
    assert all(c["busy_selected"] == c["tail_nonempty"] == "0" for c in captures)
    other = [s for s in sites if s["producer"] == "0"]
    assert {s["callback_pc"] for s in other} == {"5916", "5918", "663a", "663c"}
    assert sum(int(s["count"]) for s in other) == 16384
    blob = (out / "captured-original-banks.bin").read_bytes()
    assert len(blob) == len(captures) * 2048
    tail_records = 0
    for c in valid:
        capture = int(c["capture"]) - 1
        words = struct.unpack_from(">1024H", blob, capture * 2048)
        count = int(c["count"])
        assert 0 <= count <= 256
        for at in range(count * 4, 1024, 4):
            x, y, code, attribute = words[at:at + 4]
            assert x == attribute == 0
            # No pen/transparent assumption: even an entirely opaque16x16
            # source tile cannot touch either verified visible rectangle.
            for left, width in ((64, 384), (96, 320)):
                for flip in (False, True):
                    sx = 496 - x if flip else x
                    assert sx + 16 <= left or sx >= left + width
            tail_records += 1
    result = {
        "schema_version": 1,
        "scope": "Read-only original-source producer/selected-prefix proof on frozen continuous latch-v2 route. Metadata/prepared prefixes are host diagnostics, not yet a target implementation or cycle improvement.",
        "source_rom_sha256": pinned["source_rom_sha256"], "core_sha256": pinned["core_sha256"],
        "paired_frames_exact_to_frozen": 1800, "native_irqs": 1797,
        "observed_complete_producer_epochs": 1349,
        "all_capture_admitted_epochs": 1606, "all_capture_explicit_fallbacks": 191,
        "active_captures": 463, "active_admitted_epochs": 463,
        "active_records": stats([int(c["count"]) for c in active]),
        "active_dirty_records_since_same_bank_last_capture": stats([int(c["dirty_records"]) for c in active]),
        "active_changed_word_writes_since_same_bank_last_capture": stats([int(c["changed_writes"]) for c in active]),
        "all_epoch_records": stats([int(c["emitted_records"]) for c in epochs]),
        "selected_bank_busy_captures": 0,
        "record_write_words": sum(int(s["count"]) for s in sites if s["producer"] == "1" and s["cleanup"] == "0"),
        "cleanup_write_words": sum(int(s["count"]) for s in sites if s["cleanup"] == "1"),
        "nonproducer_write_words": 16384,
        "nonproducer_instruction_sites": ["5914", "5916", "6638", "663a"],
        "source_boundary": {
            "begin": "3B554 stores256 to FF829A after3B536..3B54A select destination. Observe bank and invalidate older metadata.",
            "count": "3B574 entry sees cursor=bankBase+8*(256-D7). The trace verifies all emitted words are sequential and total4*(256-D7). FF829A is area budget, not a record count.",
            "cleanup": "3B574 preserves pre-cleanup remaining inD6 then stores it to829C/E. It clears only X and attribute of obsolete records. All other bytes retain their source values.",
            "commit": "3B564 forces9000 for82A0 mode; otherwise3B56C flips FF8023 bit7. Admit metadata only after complete observed writer/cleanup/publication epoch.",
            "acquire": "PRE-source1554 boundary selects by previous hardware800100; source1558 publishes next FF8022 afterward. Never substitute next selector.",
            "invalidation": "Any nonproducer object write invalidates that bank. This route observes only complete clear loops590E and6632. Unobserved writer sites remain unproved.",
            "prefix": "A separate prepared prefix receives each original emitted record word directly, never by copying the final source bank. Every admitted captured prefix equals the independently prior-port-selected original bank."},
        "tail_proof": {"records": tail_records, "crop_flip_cases": tail_records * 4,
                       "crops": [{"left": 64, "width": 384}, {"left": 96, "width": 320}],
                       "flips": [0, 1],
                       "proof": "Every omitted original record hasX=0/attribute=0. Its16x16 horizontal extent is[0,16) or[496,512), disjoint from both verified crops regardless of Y/code/pen. No generic API crop or transparency assumption.",
                       "limitation": "Raw records/record-counter diagnostics differ when omitting tails; only visible composition is equivalent for these crops. Hardware/logic source memory remains intact."},
        "next_native_contract": "Per bank epoch/count/valid-busy, source-boundary invalidation/commit, prior-port acquisition. No count read is trusted without a complete observed epoch. Compact prepared descriptor lifetime must preserve the selected generation through upload.",
        "verification": "ASan/UBSan, full source RAM/sevenvideo/ports/21registers/cycles parity and five frozen files exact; all original object writes catalogued.",
        "files": {n: sha(out / n) for n in ("producer-epochs.csv", "publication-latches.csv", "object-write-sites.csv", "captured-original-banks.bin", "cadence.log", "test-publication")},
        "diagnostic_source_sha256": sha(source)}
    (out / "evidence.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: result[k] for k in ("observed_complete_producer_epochs", "all_capture_admitted_epochs", "all_capture_explicit_fallbacks", "active_records", "active_dirty_records_since_same_bank_last_capture", "tail_proof")}, indent=2))
if __name__ == "__main__": main()
