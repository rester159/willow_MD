#!/usr/bin/env python3
"""Verify transparency of the original tiles used by the 5858 scene clear."""
import hashlib
import json
from pathlib import Path
import zipfile
from collections import Counter
from cps1_gfx import assemble_region, decode_tile

root = Path(__file__).resolve().parents[1]
archive = root / "willow.zip"
with zipfile.ZipFile(archive) as source:
    graphics = assemble_region(source)
cases = [
    ("scroll1-left", 0x7020, 0x7020, 8, 0),
    ("scroll1-right", 0x7020, 0x7020, 8, 1),
    ("scroll2", 0, 0x4000, 16, 0),
    ("scroll3", 0x0a00, 0x0a00, 32, 0),
    ("object-zero", 0, 0, 16, 0),
]
rows = []
for name, source_code, decoded_code, size, layout in cases:
    counts = Counter(pen for row in decode_tile(graphics, decoded_code, size, layout)
                     for pen in row)
    if counts != {15: size * size}:
        raise SystemExit(f"Clear tile is not fully transparent: {name}: {dict(counts)}")
    rows.append(dict(region=name, source_code=source_code, decoded_code=decoded_code,
                     size=size, layout=layout, pens=dict(counts)))
report = dict(archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
              graphics_sha256=hashlib.sha256(graphics).hexdigest(), tiles=rows,
              scope="Original clear tiles only; no renderer publication or full-screen visual claim")
output = root / "build/transition-clear-graphics.json"
output.parent.mkdir(exist_ok=True)
output.write_text(json.dumps(report, indent=2) + "\n")
print(f"Clear graphics: all {sum(row['size'] ** 2 for row in rows)} pixels in five layouts use transparent CPS pen 15")
