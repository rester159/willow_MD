#!/usr/bin/env python3
"""Check native text-task writes against captured arcade Scroll1 RAM."""
import csv,json
from pathlib import Path
writes=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(open('build/arcade-native-text-writes.csv'))]
checks=[]
for frame in range(780,1021,30):
 ram=Path(f'build/arcade-trace/intro-vram-{frame}.bin').read_bytes()
 emitted=[w for w in writes if w['frame']<=frame]
 latest={w['address']:w for w in emitted}
 for address,w in latest.items():
  at=address-0x900000
  actual=int.from_bytes(ram[at:at+2],'big'),int.from_bytes(ram[at+2:at+4],'big')
  assert actual==(w['code'],w['attribute']),(frame,hex(address),actual,w)
 checks.append({'frame':frame,'glyph_writes_checked':len(latest)})
print(f'Arcade text: {sum(c["glyph_writes_checked"] for c in checks)} glyph/code/attribute comparisons across {len(checks)} captures passed')
Path('build/arcade-text-parity.json').write_text(json.dumps(checks,indent=2)+'\n')
