#!/usr/bin/env python3
"""Read-only source-route WRAM ownership proof; no native renderer installed."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "build/native-object-publication/checkpoint-compact-v1"
OUT = ROOT / "build/native-render-memory/route-v1"
IDENTICAL = (
    "frames.csv", "irqs.csv", "irq-ring.bin", "captured-original-banks.bin",
    "metadata-acquisitions.csv", "publication-latches.csv", "producer-epochs.csv",
    "headroom-events.csv", "headroom-windows.csv",
)
DEPENDENCIES = {
    "src/willow_arcade_player_motion.h": "b4c66a8d7437d8c8e55083235db3319be0bfd10dfe78e20f54ff356b5d66ca83",
    "tools/vendor/libretro.h": "c928d8f176b4e4bc45ee2a41ef236d25cc19e9bc2abc2e2e4c32cf08d6423801",
}
COUNTS = ("source_reads", "source_writes", "native_reads", "native_writes")


def require(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + "\n")


def linker_probes():
    lines = []
    for size in (3072, 4096, 4100):
        stem = OUT / f"linker-probe-{size}"
        stem.with_suffix(".S").write_text(f'.section .native_renderer_bss,"aw",@nobits\n.space {size}\n')
        subprocess.run(["m68k-elf-as", "-m68000", str(stem.with_suffix(".S")), "-o", str(stem.with_suffix(".o"))], check=True)
        result = subprocess.run(["m68k-elf-ld", "-T", "tools/native_render_memory_proposal.ld", str(stem.with_suffix(".o")), "-o", str(stem.with_suffix(".elf"))], cwd=ROOT, capture_output=True, text=True)
        require((result.returncode == 0) == (size <= 4096), f"Linker capacity guard failed: {size}")
        lines.append(f"{size} bytes: " + ("accepted" if result.returncode == 0 else "rejected") + "\n" + result.stderr)
        if result.returncode == 0:
            symbols = subprocess.check_output(["m68k-elf-nm", "-n", str(stem.with_suffix(".elf"))], text=True)
            require("00ff7800 B __native_renderer_stack_top" in symbols, "Wrong native stack top")
            lines.append(symbols)
    (OUT / "linker-probes.log").write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--core", type=Path, default=Path.home() / "Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    checkpoint = json.loads((BASE / "checkpoint.json").read_text())
    for name, digest in checkpoint["files"].items():
        require(sha(BASE / name) == digest, f"Changed compact input: {name}")
    for name, digest in DEPENDENCIES.items():
        require(sha(ROOT / name) == digest, f"Changed compiled dependency: {name}")
    baseline = json.loads((BASE / "evidence.json").read_text())
    require(sha(args.core) == baseline["core_sha256"], "Installed core differs")
    require(sha(ROOT / "build/reference_maincpu.bin") == baseline["source_rom_sha256"], "Original ROM differs")
    if not args.no_run:
        linker_probes()
        subprocess.run([
            "clang", "-Wall", "-Wextra", "-Werror", "-O2", "-fstrict-aliasing",
            "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
            "tools/test_native_render_memory.c", "-o", str(OUT / "test-memory"),
        ], cwd=ROOT, check=True)
        with (OUT / "guard-selftest.log").open("w") as log:
            subprocess.run([str(OUT / "test-memory"), "--guard-selftest"], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        with (OUT / "cadence.log").open("w") as log:
            subprocess.run([str(OUT / "test-memory"), str(args.core), "1800", "--coin-route"], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    require("24 positive/negative boundary/reset cases pass" in (OUT / "guard-selftest.log").read_text(), "Guard cases missing")
    linker_log = (OUT / "linker-probes.log").read_text()
    for expected in ("3072 bytes: accepted", "4096 bytes: accepted", "4100 bytes: rejected", "00ff7800 B __native_renderer_stack_top"):
        require(expected in linker_log, f"Missing linker check: {expected}")
    require("1800 paired physicalGenesis frames" in (OUT / "cadence.log").read_text(), "Incomplete route")
    for name in IDENTICAL:
        require((OUT / name).read_bytes() == (BASE / name).read_bytes(), f"Route observer changed {name}")
    memory = rows(OUT / "work-ram-byte-accesses.csv")
    require(len(memory) == 65536, "Incomplete WRAM coverage")
    for offset, row in enumerate(memory):
        require(int(row["offset"], 16) == offset, "Nonsequential byte map")
        require(int(row["boot_clear_writes"]) == 1, f"Bootstrap byte coverage {offset:04x}")
        for key in COUNTS:
            require(int(row[key]) >= 0, "Invalid access count")
    lifetime = json.loads((OUT / "memory-lifetime.json").read_text())
    require(lifetime["schema_version"] == 2 and lifetime["boot_clears"] == 1, "Unexpected bootstrap clear count")
    require(lifetime["ownership_active"] == 1 and lifetime["guard_rejections"] == 0, "Ownership guard failed")
    require(lifetime["last_boot_clear_start_master"] < lifetime["last_boot_clear_end_master"] < lifetime["first_pre_irq_allocation_boundary_master"], "Unsafe activation timing")
    require(lifetime["guarded_ranges"] == [[0xff4000, 0xff5000], [0xff7000, 0xff7800]], "Guarded placement differs")
    placements = []
    for name, start, end in (("renderer_globals_and_canary_padding", 0x4000, 0x5000), ("independent_native_stack", 0x7000, 0x7800)):
        totals = {key: sum(int(row[key]) for row in memory[start:end]) for key in COUNTS}
        require(not any(totals.values()), f"Observed conflict in {name}: {totals}")
        placements.append({
            "name": name, "start": 0xff0000 + start, "end_exclusive": 0xff0000 + end,
            "bytes": end - start, "non_bootstrap_access_counts": totals,
            "initial_bootstrap_byte_writes": end - start,
        })
    gaps = []
    start = None
    for offset in range(65537):
        clean = offset < 65536 and not any(int(memory[offset][key]) for key in COUNTS)
        if clean and start is None:
            start = offset
        if not clean and start is not None:
            if offset - start >= 64:
                gaps.append({"start": 0xff0000 + start, "end_exclusive": 0xff0000 + offset, "bytes": offset - start})
            start = None
    captures = rows(OUT / "publication-latches.csv")
    events = rows(OUT / "headroom-events.csv")
    active = [row for row in captures if row["manager"] == row["scene"] == "2"]
    commits = [row for row in events if row["event"] == "main_wait_sleep_committed"]
    active_commits = [row for row in commits if row["manager"] == row["scene"] == "2"]
    # Event spelling is pinned by the reference artifact, not inferred timing.
    require(len(captures) == 1797 and len(active) == 464, "Changed source route coverage")
    require(len(commits) == 1491 and len(active_commits) == 465, "Changed source sleep observations")
    placement = {
        "schema_version": 1,
        "scope": "Guarded allocation proposal for the standalone new renderer on the observed 1800-frame source route; not a whole-game ownership proof or live renderer admission.",
        "regions": placements,
        "stack_top": 0xff7800,
        "source_task_stacks": "Retain all original 128-byte stacks with tops FF0290..FF0A10; no reuse of unobserved slots.",
        "existing_latch": {"first_chunk": 0xff2660, "chunks": 32, "stride": 128, "chunk_bytes": 64, "last_end_exclusive": 0xff3620},
        "activation": lifetime,
        "reset_contract": "Do not initialize/retain renderer state during source 6F2 full-WRAM clear. Activate only after its completion. An observed new clear while ownership is active is rejected; no post-activation reset was injected in this route.",
        "linker_contract": "Collect only explicit new renderer globals; derive occupied bytes from the integrated link map and guard every padding byte. Proposal does not include retired SGDK allocations.",
        "stack_contract": "Native wrapper must preserve original SP/context, switch to the independent stack, and restore exactly; source task stacks are too small. Check bottom canary/high-water and bounds in the eventual integrated run. This observer does not execute the new renderer or establish its stack usage.",
        "observation_contract": "Reference effective bus reads/writes include original source and existing target helpers; candidate full-frame state/cycles remain exact. Initial sequential full-WRAM clear is counted separately, never hidden. No target writes or host register writes were added.",
    }
    save("placement.json", placement)
    save("observed-unused-spans.json", {
        "scope": "Observed gaps excluding the initial full-WRAM clear; not all are reusable. Only the two guarded placement regions are proposed.",
        "minimum_bytes": 64, "spans": gaps,
    })
    evidence = {
        "schema_version": 1, "scope": placement["scope"],
        "source_rom_sha256": baseline["source_rom_sha256"], "core_sha256": baseline["core_sha256"],
        "compact_checkpoint_sha256": sha(BASE / "checkpoint.json"),
        "target_products": json.loads((BASE / "manifest.json").read_text())["products"],
        "paired_frames": 1800, "captures": len(captures), "active_captures": len(active),
        "source_main_sleep_commits": len(commits), "commits_with_active_source_state": len(active_commits),
        "commit_scope": "465 sleep commits observed with manager/scene 2/2 includes the transition into gameplay; 464 pre-IRQ acquisitions have 2/2 state. Neither count is a renderer FPS claim.",
        "work_ram_bytes_observed": len(memory), "initial_full_clear_bytes": 65536,
        "guard_cases": 24, "linker_capacity_cases": 3, "regions": placements, "lifetime": lifetime,
        "identical_cadence_files": {name: sha(OUT / name) for name in IDENTICAL},
        "compiled_dependencies": DEPENDENCIES,
        "files": {name: sha(OUT / name) for name in ("work-ram-byte-accesses.csv", "memory-lifetime.json", "placement.json", "observed-unused-spans.json", "guard-selftest.log", "linker-probes.log", "test-memory", "cadence.log")},
        "tools": {name: sha(ROOT / name) for name in ("tools/test_native_render_memory.c", "tools/verify_native_render_memory.py", "tools/native_render_memory_proposal.ld")},
    }
    save("evidence.json", evidence)
    print(json.dumps({key: evidence[key] for key in ("paired_frames", "captures", "active_captures", "guard_cases", "regions", "lifetime")}, indent=2))


if __name__ == "__main__":
    main()
