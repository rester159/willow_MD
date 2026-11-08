#!/usr/bin/env python3
"""Read-only bus timing of the frozen continuous latch-v2 route; no renderer admission."""
import argparse
import csv
import hashlib
import json
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FROZEN = ROOT / "build/native-cadence/checkpoint-latch-v2"
MASTER_HZ = 53_693_175

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def rows(path):
    return list(csv.DictReader(path.open()))

def distribution(values):
    return {"samples": len(values), "minimum": min(values), "mean": statistics.mean(values),
            "maximum": max(values)}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/native-cadence/headroom-v1")
    parser.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    parser.add_argument("--no-run", action="store_true", help="Summarize an already completed run; all pinned inputs/output comparisons still checked.")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    base = json.loads((FROZEN / "evidence.json").read_text())
    assert sha(ROOT / "build/reference_maincpu.bin") == base["source_rom_sha256"]
    assert sha(args.core) == base["core_sha256"]
    inputs = ["program.bin", "platform.bin", "platform-original.bin", "platform-addresses.h",
              "tools/test_native_cadence_latch.c"]
    for name in inputs:
        assert sha(FROZEN / name) == base["files"][name], name
    assert sha(ROOT / "src/willow_arcade_player_motion.h") == base["files"]["src/willow_arcade_player_motion.h"]
    source = ROOT / "tools/test_native_cadence_headroom.c"
    binary = out / "test-headroom"
    if not args.no_run:
        subprocess.run(["clang", "-Wall", "-Wextra", "-Werror", "-O1", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", str(source), "-o", str(binary)], check=True, cwd=ROOT)
        with (out / "cadence.log").open("w") as log:
            subprocess.run([str(binary), str(args.core), "1800", "--coin-route", str(out)],
                           cwd=ROOT, check=True, stdout=log, stderr=subprocess.STDOUT)
    unchanged = ["frames.csv", "irqs.csv", "irq-ring.bin", "pointer-sites.csv", "oracle.bin"]
    for name in unchanged:
        assert (out / name).read_bytes() == (FROZEN / name).read_bytes(), name
    events = rows(out / "headroom-events.csv")
    windows = rows(out / "headroom-windows.csv")
    irqs = rows(out / "irqs.csv")
    starts = [e for e in events if e["event"] == "irq_first_native_store"]
    begins = [e for e in events if e["event"] == "main_wait_movem_first_store"]
    commits = [e for e in events if e["event"] == "main_wait_sleep_committed"]
    assert len(starts) == 1797 and len(begins) == len(commits) == 1489
    assert len(windows) == 1796
    # Align every reference bus observation with the independently executed
    # native IRQ observer: interrupted PC/SR and physical retro_run frame.
    for observed, native in zip(starts, irqs, strict=True):
        assert observed["irq"] == native["irq"] and observed["frame"] == native["frame"]
        assert int(observed["arg0"], 16) == int(native["interrupted_pc"], 16)
        assert int(observed["arg1"], 16) == int(native["saved_sr"], 16)
        assert observed["manager"] == native["manager"] and observed["scene"] == native["scene"]
    for w, start, next_start in zip(windows, starts[:-1], starts[1:], strict=True):
        assert w["irq_entry_master"] == start["absolute_master_cycles"]
        assert w["next_irq_master"] == next_start["absolute_master_cycles"]
        assert int(w["next_irq_master"]) > int(w["irq_entry_master"])
        native = irqs[int(w["irq"])]
        assert int(w["main_waits"]) == int(native["gameplay_wait_mask"], 16).bit_count()
    active = [w for w in windows if w["end_manager"] == w["end_scene"] == "2"]
    completed = [w for w in active if int(w["main_waits"])]
    missing = [w for w in active if not int(w["main_waits"])]
    assert len(active) == 463 and len(completed) == 462 and len(missing) == 1
    assert int(missing[0]["irq"]) + 1 == 1335
    # A terminal partial window has no next IRQ in this run; it is deliberately
    # excluded, rather than extrapolating the next IRQ or using end-of-run time.
    upper = [int(w["next_irq_master"]) - int(w["main_commit_master"]) for w in completed]
    irq_to_commit = [int(w["main_commit_master"]) - int(w["irq_entry_master"]) for w in completed]
    minimum = completed[upper.index(min(upper))]
    productive = [w for w in completed if any(int(w[f]) for f in
                  ("post_video_writes", "post_port_writes", "post_queue_consumes"))]
    assert len(productive) == 1 and productive[0]["irq"] == "1335"
    assert productive[0]["post_video_writes"] == "48"
    dispatches = {f"{n:02x}": sum(int(w[f"post_dispatch_{n:02x}"]) for w in completed)
                  for n in range(0, 512, 32)}
    assert dispatches["a0"] == 11709 and dispatches["100"] == 1
    release = [e for e in events if e["event"] == "post_main_dynamic_task_released" and e["irq"] == "1335"]
    assert len(release) == 1 and int(release[0]["arg0"], 16) == 0xff0100
    result = {
        "schema_version": 1,
        "scope": "Read-only source-bus timing of frozen seven-region latch-v2 continuous scheduler. No new target instructions, renderer, CPS-speed equivalence or shipping admission.",
        "source_rom_sha256": base["source_rom_sha256"],
        "core_sha256": base["core_sha256"],
        "clock": {"name": "Genesis NTSC", "master_hz": MASTER_HZ, "master_cycles_per_frame": 896040,
                  "libretro_hz": 59.922743404,
                  "not_arcade_clock": "Original CPS 10MHz / 59.63740458Hz speed remains a separate requirement."},
        "paired_frames": 1800, "irq_entries": 1797, "completed_irq_intervals": len(windows),
        "unchanged_frozen_outputs": {name: sha(out / name) for name in unchanged},
        "active_end_state": {"manager": 2, "scene": 2, "intervals": len(active),
                             "completed_main_waits": len(completed),
                             "missing_wait_next_irqs": [int(w["irq"]) + 1 for w in missing],
                             "missing_transition_interrupted_source_pc": "403e2"},
        "event_definition": {
            "main_begin": "Original1750 MOVEM first register-store callback: saved caller932C, current task60, D0 low byte1.",
            "main_commit": "Original177C MOVE.L exception PC into task+4 final word callback, PC1780: task60 state0101, savedPC1756, originalUSP with saved caller932C.",
            "irq_boundary": "First native wrapper MOVE.W SR,-SP bus callback at10006A; genuine saved hardware SR/PC checked against the independent target IRQ observer. This follows hardware exception entry and any interrupt masking latency.",
            "time": "frame_index*896040 + installed-core CPU master-cycle accumulator at bus callback. These are callback times, not sub-instruction hardware pin timestamps.",
            "upper_bound": "main_commit→next native IRQ-entry includes subsequent source work and interrupt-entry latency. It is not idle duration or a guaranteed available render budget."},
        "main_commit_to_next_irq_upper_bound_master": distribution(upper),
        "main_commit_to_next_irq_upper_bound_ms": distribution([v * 1000 / MASTER_HZ for v in upper]),
        "irq_entry_to_main_commit_ms": distribution([v * 1000 / MASTER_HZ for v in irq_to_commit]),
        "minimum_upper_bound_interval": {"starting_irq": int(minimum["irq"]), "frame": int(minimum["start_frame"]),
                                         "next_irq": int(minimum["irq"]) + 1},
        "post_main_work": {
            "task_dispatches": {k: v for k, v in dispatches.items() if v},
            "video_word_writes": sum(int(w["post_video_writes"]) for w in completed),
            "port_writes": sum(int(w["post_port_writes"]) for w in completed),
            "service_queue_consumptions": sum(int(w["post_queue_consumes"]) for w in completed),
            "productive_interval": {"irq": 1335, "frame": 1337, "spawned_task": "100", "entry": "5baa",
                                    "consumer": "1E2C reads pending queue word; 1DB6 consumes it; TRAP7 creates task100; 5BF2/5BF4 write24 Scroll1 tile/attribute pairs; TRAP9 returns slot to free list at1684.",
                                    "last_video_word_master": int(productive[0]["last_video_write_master"]),
                                    "task_release_master": int(release[0]["absolute_master_cycles"]),
                                    "commit_to_task_release_ms": (int(release[0]["absolute_master_cycles"]) - int(productive[0]["main_commit_master"])) * 1000 / MASTER_HZ},
            "a0_caution": "A0 intentionally remains ready and repeatedly checks restart/free-task/service-queue state. No scheduler-spin or ready-yield is declared idle or removed.",
            "sites_scope": "post-main-sites.csv includes all post-wait accesses, split by manager2/scene2 at access; it includes the final partial interval. Completed-window totals above exclude it.",
            "exception_writes": "The genuine next IRQ exception frame is pushed before the native-entry marker. Such final stack writes retain the interrupted source PC and are included in RAM access counts; they are not incorrectly classified as source instructions writing those addresses."},
        "renderer_target_interpretation": "On this short route, the tightest completed-main upper bound is5.774ms before subtracting remaining tasks/interrupt overhead or renderer integration costs. A renderer must fit below this bound if retaining the measured source timing. This does not certify other routes or CPS-clock behavior.",
        "inputs": {name: sha(FROZEN / name) for name in inputs},
        "diagnostic_source_sha256": sha(source),
        "verification": "ASan/UBSan paired execution; full sourceRAM/sevenvideo/ports/21CPUregisters/native observations and residual cycles match. Frozen frame outputs and all1797 independent native IRQ records are exact."}
    result["files"] = {name: sha(out / name) for name in
                       ["headroom-events.csv", "headroom-windows.csv", "post-main-sites.csv", "cadence.log", "test-headroom"]}
    (out / "evidence.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: result[k] for k in ("paired_frames", "active_end_state", "main_commit_to_next_irq_upper_bound_ms", "post_main_work")}, indent=2))

if __name__ == "__main__":
    main()
