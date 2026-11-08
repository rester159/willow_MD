#!/usr/bin/env python3
"""cyclescan.py OUT.json DUMP...: the scroll2 palette entries that change
across consecutive per-frame CPS dumps (a palette cycle, e.g. stage 1's
waterfall), as {"group,pen": [CPS word per state]} plus the frames between
states. Input: lockstep VIDDUMP files of one cycle period or more."""
import json, sys
from pathlib import Path

out, files = sys.argv[1], sorted(sys.argv[2:])
pals = []
for f in files:
    raw = Path(f).read_bytes(); d = raw[0x80:]
    pb = (int.from_bytes(raw[0x0A:0x0C], "big") << 8) - 0x900000 + 0x800    # scroll2 bank
    pals.append([int.from_bytes(d[pb + i * 2:][:2], "big") for i in range(32 * 16)])
steps = [k for k in range(1, len(pals)) if pals[k] != pals[k - 1]]
gap = steps[1] - steps[0]
assert all(b - a == gap for a, b in zip(steps, steps[1:])), steps
states = [pals[steps[0] - 1]] + [pals[k] for k in steps]
cyc = {}
for i in range(32 * 16):
    seq = [s[i] for s in states]
    if len(set(seq)) > 1:
        cyc[f"{i // 16},{i % 16}"] = seq
period = next(n for n in range(1, len(states)) if all(v[n:] == v[:-n] for v in cyc.values()))
cyc = {k: v[:period] for k, v in cyc.items()}
Path(out).write_text(json.dumps({"frames_per_step": gap, "states": period, "entries": cyc}, indent=1))
print(f"{len(cyc)} entries, {period} states, {gap} frames a step")
