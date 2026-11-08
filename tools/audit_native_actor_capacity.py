#!/usr/bin/env python3
"""Report actor-storage gaps in the current renderer; never claims game completeness.

Run intro_rom_smoke against the current ROM first and provide that log. Heap
figures include allocator headers and are an upper bound on usable storage.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("smoke_log", type=Path)
args = parser.parse_args()
spec_text = (root / "src/willow_arcade_actor_pool.h").read_text()
spec_body = re.search(r"specs\[9\]=\{(.*?)\};", spec_text, re.S)
if not spec_body:
    raise SystemExit("Cannot locate canonical actor pool specifications")
specs = re.findall(r"\{(0x[0-9a-f]+),(\d+),(\d+)\}", spec_body[1])
if len(specs) != 9:
    raise SystemExit("Expected nine canonical actor classes")
renderer = (root / "native/intro_probe/src/main.c").read_text()
classes = re.findall(r"const u16 classes\[(\d+)\]=\{([0-9,]+)\};", renderer)
if not classes or "class18Records[8*128]" not in renderer:
    raise SystemExit("Renderer allocation scheme changed; re-audit required")
parsed = [tuple(map(int, values.split(","))) for _, values in classes]
if any(len(values) != int(count) or len(set(values)) != len(values)
       for (count, _), values in zip(classes, parsed)) or any(values != parsed[0] for values in parsed):
    raise SystemExit("Renderer allocation and dispatch class tables disagree")
present = set(parsed[0]) | {18}
if not present <= set(range(2, 19, 2)):
    raise SystemExit("Unexpected actor class in renderer")
log = args.smoke_log.read_text()
heap = re.findall(r"Native heap blocks: (\d+) bytes total, (\d+) bytes largest", log)
if len(heap) != 1 or "Native class-18 pool:" not in log or "Native spawn entry:" not in log:
    raise SystemExit("Need a completed current-ROM smoke log with actor and heap checks")
rows = []
for index, (base, count, stride) in enumerate(specs):
    class_id = (index + 1) * 2
    rows.append(dict(class_id=class_id, source_base=base, capacity=int(count),
                     record_bytes=int(stride), total_bytes=int(count)*int(stride),
                     renderer_storage_present=class_id in present))
missing = sum(row["total_bytes"] for row in rows if not row["renderer_storage_present"])
largest_missing = max((row["total_bytes"] for row in rows if not row["renderer_storage_present"]), default=0)
free, largest = map(int, heap[0])
report = dict(
    rom_sha256=hashlib.sha256((root / "native/intro_probe/out/rom.bin").read_bytes()).hexdigest(),
    evidence_log=str(args.smoke_log.resolve()),
    actor_classes=rows,
    missing_record_bytes=missing,
    heap_free_including_headers=free,
    largest_coalesced_heap_block_including_headers=largest,
    minimum_additional_bytes_needed_for_records=max(0, missing-free),
    largest_missing_contiguous_record_array=largest_missing,
    largest_free_block_shortfall_for_that_array=max(0, largest_missing-largest),
    each_missing_array_fits_largest_block=all(
        row["total_bytes"] < largest for row in rows if not row["renderer_storage_present"]),
    all_actor_record_storage_present=missing == 0,
    limitations=["Current renderer source and supplied smoke log must match the ROM.",
                 "Total free bytes do not establish a feasible allocation layout; free-block headers and simultaneous allocations also consume space.",
                 "Missing indices, pool descriptors, task/global state and audio are not budgeted.",
                 "Presence of storage does not establish actor execution or campaign coverage."],
)
print(json.dumps(report, indent=2))
