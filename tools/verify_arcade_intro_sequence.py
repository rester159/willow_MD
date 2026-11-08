#!/usr/bin/env python3
"""Source state and Scroll1 checks through the unskipped intro handoff boundary."""
from pathlib import Path
import subprocess,csv,json
from cps1_program import assemble_maincpu
p=assemble_maincpu(Path('willow.zip'))
anchors=[(0xa360,'4a2d02a66692536d000a668c3b7c0001000a4a6d040c6680'),(0xa38a,'3b7c0078000a546d0008323c0419343c0306'),(0xa3ae,'4a2d02a6662e536d000a6628'),(0xa3ee,'4a2d02a666ee546d00081b7c000102a63b7c001e000a'),(0xa41c,'4a2d02a666c0536d000a66ba4eb866421b7c00010348546d00043b7c00020008'),(0x6642,'41f900910000303c03ff323c7020740060aa'),(0x7f12,'286e001c30190640700030c030d40640001030c030d441e800780640fff130c030d40640001030c030d44e75')]
for a,h in anchors:assert p[a:a+len(bytes.fromhex(h))]==bytes.fromhex(h),hex(a)
subprocess.run(['python3','tools/native_intro_text.py'],check=True)
subprocess.run(['clang','-Wall','-Wextra','-Werror','tools/test_arcade_intro_sequence.c','-o','build/test_arcade_intro_sequence'],check=True)
subprocess.run(['build/test_arcade_intro_sequence'],check=True)
writes=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(open('build/native-intro-sequence-writes.csv'))]
latest={};i=0;report=[]
# Handoff additionally invokes HUD/gameplay helpers; its subsequent RAM is not
# claimed to equal the preceding clear in this intro-only translation.
for frame in range(1140,1588):
 while i<len(writes) and writes[i]['frame']<=frame:
  w=writes[i];latest[w['address']]=w;i+=1
 ram=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 for address,w in latest.items():
  at=address-0x910000
  got=int.from_bytes(ram[at:at+2],'big'),int.from_bytes(ram[at+2:at+4],'big')
  assert got==(w['code'],w['attribute']),(frame,hex(address),got,w)
 report.append({'frame':frame,'tiles_checked':len(latest)})
result={'manager_frames':898,'handoff_frame':1588,'source_anchors':len(anchors),'tile_attribute_comparisons':sum(r['tiles_checked'] for r in report),'frames':report}
Path('build/arcade-intro-sequence-parity.json').write_text(json.dumps(result,indent=2)+'\n')
print(f"Intro text: {result['tile_attribute_comparisons']} code/attribute checks across {len(report)} snapshots passed")
