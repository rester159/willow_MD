#!/usr/bin/env python3
"""Verify native panel writes against every source expansion checkpoint."""
from pathlib import Path
import subprocess,csv,json
from cps1_program import assemble_maincpu
from cps1_text_task import decode
p=assemble_maincpu(Path('willow.zip'))
anchors=[(0x78f4,'7000102e0010323b000a4ebb1006'),(0x7bea,'532e00156700ff7a4e75'),(0x7bde,'542e00141d7c000400154e75'),(0x7bf4,'6100fd1c422d02a64ef81636'),(0x7c00,'30190640700030c030c30640001030c030c341e800780640fff130c030c30640001030c030c34e75'),(0x7c28,'30190640700030c030c30640001030c030c341e8ff780640fff130c030c30640001030c030c34e75')]
for a,h in anchors:assert p[a:a+len(bytes.fromhex(h))]==bytes.fromhex(h),hex(a)
m=decode(p,0x419,0x0306)
assert m['message_index']==25 and m['handler']==4
assert m['stream']==b"WILLOW, BE CAREFUL. BAVMORDA'S FINGERS&PENETRATE OUTSIDE OF THE VILLA./"
for task,style in [(0x640,0x1412),(0x641,0x1413)]:
 d=decode(p,task,style);assert d['handler']==6 and d['stream'][-2:]==b'\x00/'
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_text_box.c','-o','build/test_arcade_text_box'],check=True)
with open('build/native-text-box-writes.csv','w') as f:subprocess.run(['build/test_arcade_text_box'],stdout=f,check=True)
writes=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(open('build/native-text-box-writes.csv'))]
latest={};report=[]
for phase in range(5):
 for w in writes:
  if w['phase']==phase:latest[w['address']]=w
 frame=1153+phase*4
 ram=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 for address,w in latest.items():
  at=address-0x910000
  got=int.from_bytes(ram[at:at+2],'big'),int.from_bytes(ram[at+2:at+4],'big')
  assert got==(w['code'],w['attribute']),(frame,hex(address),got,w)
 report.append({'frame':frame,'phase':phase,'tiles_checked':len(latest)})
Path('build/arcade-text-box-parity.json').write_text(json.dumps(report,indent=2)+'\n')
print(f"Panel: {len(writes)} source-derived writes, {sum(r['tiles_checked'] for r in report)} tile/attribute checks across five expansion phases passed")
