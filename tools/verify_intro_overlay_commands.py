#!/usr/bin/env python3
"""Check native predecoded panel commands against the direct C translation."""
from pathlib import Path
import subprocess, json, csv, io
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_text_box.c','-o','build/test_arcade_text_box'],check=True)
source=subprocess.run(['build/test_arcade_text_box'],capture_output=True,text=True,check=True)
rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(io.StringIO(source.stdout))]
assets=json.loads(Path('build/intro-overlay-assets.json').read_text())
codes={(r[0],r[1]):r[2:] for r in assets['records']}
visible=0
for phase,(start,count,total,mask) in enumerate(assets['phases']):
 writes=[r for r in rows if r['phase']==phase];assert len(writes)==total
 expected=[];dirty=0
 for r in writes:
  col=(r['address']-0x910000)//128;row=((r['address']-0x910000)%128)//4
  if 4<=col<44 and 2<=row<30:
   pattern=codes[r['code'],r['attribute']&31][col&1]
   flags=(r['attribute']&96)<<6 if pattern else 0
   expected.append([(row-2)*assets['row_stride']+col-4,pattern|flags]);dirty|=1<<(row-2)
 assert expected==assets['commands'][start:start+count],phase
 assert dirty==mask,phase
 visible+=count
report={'source_writes':len(rows),'visible_commands':visible,'phases':len(assets['phases'])}
Path('build/intro-overlay-command-parity.json').write_text(json.dumps(report,indent=2)+'\n')
print(f"Native panel batches: {len(rows)} source writes, {visible} cropped commands, all 5 phases match direct C translation")
