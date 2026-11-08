#!/usr/bin/env python3
"""Check dialogue animation, lettering and completion against an unskipped trace."""
from pathlib import Path
import subprocess,csv,json
subprocess.run(['python3','tools/native_intro_text.py'],check=True)
subprocess.run(['python3','tools/verify_arcade_text_box.py'],check=True)
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_dialogue.c','-o','build/test_arcade_dialogue'],check=True)
subprocess.run(['build/test_arcade_dialogue'],check=True)
writes=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(open('build/native-dialogue-writes.csv'))]
latest={};i=0;report=[]
for frame in range(1153,1497):
 while i<len(writes) and writes[i]['frame']<=frame:
  w=writes[i];latest[w['address']]=w;i+=1
 ram=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 for address,w in latest.items():
  at=address-0x910000
  got=int.from_bytes(ram[at:at+2],'big'),int.from_bytes(ram[at+2:at+4],'big')
  assert got==(w['code'],w['attribute']),(frame,hex(address),got,w)
 report.append({'frame':frame,'tiles_checked':len(latest)})
Path('build/arcade-dialogue-parity.json').write_text(json.dumps({'writes':len(writes),'frames':report},indent=2)+'\n')
print(f"Dialogue: {sum(r['tiles_checked'] for r in report)} tile/code/attribute checks across {len(report)} frames passed")
