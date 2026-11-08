#!/usr/bin/env python3
"""Report observed boot-bus coverage; this is not a full-game RAM budget."""
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
main_path = ROOT / 'native/intro_probe/src/main.c'
main = main_path.read_text()
resolver = main.split('static u8 *sourceFixedRecord(u16 at,u16 bytes)\n', 1)[1].split('\nstatic u16 introQueueWord', 1)[0]
# Extract byte ranges from the actual fixed-record resolver. A one-byte access
# is contained in each nonempty range; no record is inferred across a gap.
ranges = [(int(a, 16), int(b, 16)) for a, b in re.findall(
    r'if\(at>=(0x[0-9a-f]+) && at<(0x[0-9a-f]+) && bytes<=', resolver)]
for end in re.findall(r'if\(at<(0x[0-9a-f]+) && bytes<=', resolver):
    ranges.append((0, int(end, 16)))
if len(ranges) != 14:
    raise SystemExit('Fixed-record resolver changed; review coverage extraction')
boot_path = ROOT / 'src/willow_native_boot_globals.h'
boot_source = boot_path.read_text()
if 'return willowNativeBootGlobalsResolve(&sourceBootGlobals,at,bytes)' not in resolver:
    raise SystemExit('Boot-global mapper is not connected')
boot_ranges = re.findall(r'\{(0x[0-9a-f]+),(\d+),(\d+)\}', boot_source)
retained_offsets = set()
for start, count, offset in boot_ranges:
    lo, size = int(start, 16), int(count)
    slots = set(range(int(offset), int(offset) + size))
    if retained_offsets & slots or not (lo < 0x8402 or lo >= 0xf442):
        raise SystemExit('Boot-global layout/routing changed; review extraction')
    ranges.append((lo, lo+size))
    retained_offsets.update(slots)
if retained_offsets != set(range(179)):
    raise SystemExit('Boot-global extent changed; review coverage')

scores_path = ROOT / 'src/willow_native_high_scores.h'
if 'willowHighScorePointerRead(&sourceHighScores' not in main or 'willowHighScorePointerWriteWord(&sourceHighScores' not in main:
    raise SystemExit('High-score pointer bus is not connected')
# Restricted pointer writes are sufficient for the original startup values
# in this trace. This does not claim support for arbitrary pointer mutations.
ranges.append((0xf016, 0xf03e))
access = {}
inputs = []
for region in range(5):
    path = ROOT / f'build/reset-boot-access-{region}.csv'
    inputs.append({'path': str(path.relative_to(ROOT)),
                   'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    with path.open() as stream:
        for row in csv.DictReader(stream):
            at = int(row['address'], 16)
            flags = [int(row[name]) for name in
                     ('startup_read', 'startup_write', 'runtime_read', 'runtime_write')]
            access[at] = [a | b for a, b in zip(access.get(at, [0]*4), flags)]
missing = sorted(at for at in access if not any(lo <= at < hi for lo, hi in ranges))
# This boot path does not touch the other two native bus routes. Fail closed if
# it expands there: their availability and class-18 lifetime need a fresh audit.
other_routes = [(0x9102, 0x9356), (0x95ce, 0x96be), (0x97fe, 0x9842),
                (0x98c6, 0xefc6), (0xf456, 0xf466), (0xf486, 0xf48e),
                (0xf490, 0xf890), (0xf900, 0xf97c)]
if any(lo <= at < hi for at in missing for lo, hi in other_routes):
    raise SystemExit('Boot footprint reaches pool/visibility routing; extend audit')
intervals = []
for at in missing:
    if not intervals or at != intervals[-1][-1] + 1:
        intervals.append([at])
    else:
        intervals[-1].append(at)
report = {
    'scope': '64 reset-initialized boot-to-attract cases across five video regions; '
             'excludes full-RAM reset clear, fixture accesses, CPS video and palette buses',
    'limitation': 'Observed byte lower bound, not a storage allocation or full campaign budget',
    'main_sha256': hashlib.sha256(main_path.read_bytes()).hexdigest(),
    'boot_globals_sha256': hashlib.sha256(boot_path.read_bytes()).hexdigest(),
    'high_scores_sha256': hashlib.sha256(scores_path.read_bytes()).hexdigest(),
    'inputs': inputs,
    'observed_bytes': len(access),
    'mapped_bytes': len(access)-len(missing),
    'missing_observed_bytes': len(missing),
    'missing_runtime_bytes': sum(bool(access[a][2] or access[a][3]) for a in missing),
    'missing_ranges': [{'start': f'{r[0]:04x}', 'end_inclusive': f'{r[-1]:04x}',
                        'bytes': len(r), 'runtime': any(access[a][2] or access[a][3] for a in r)}
                       for r in intervals],
}
output = ROOT / 'build/reset-boot-storage.json'
output.write_text(json.dumps(report, indent=2) + '\n')
print(f"Boot bus: {report['observed_bytes']} observed bytes, {report['mapped_bytes']} mapped, "
      f"{report['missing_observed_bytes']} missing ({report['missing_runtime_bytes']} used during scheduling).")
print('Observed lower bound only; unobserved state, later paths and alignment still need a full-game budget.')
