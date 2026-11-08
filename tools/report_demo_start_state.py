"""Report observed low-global gaps; excludes direct-resolver writes and video."""
from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parents[1]
access=(root/'build/demo-start-bus-access.bin').read_bytes()
if len(access)!=65536 or any(x>3 for x in access):raise SystemExit('Invalid observed access bitset')
owned=set(range(0x200))|set(range(0x200,0x210))|set(range(0xa10,0xa30))
for at,count,offset in re.findall(r'\{(0x[0-9a-f]+),(\d+),(\d+)\}',(root/'src/willow_native_boot_globals.h').read_text()):
 owned.update(range(int(at,16),int(at,16)+int(count)))
supplemental=set()
header=root/'src/willow_native_demo_start_state.h'
main=(root/'native/intro_probe/src/main.c').read_text()
if header.exists() and 'willowNativeDemoStartStateResolve(SOURCE_DEMO_START_STATE,at,bytes)' in main:
 text=header.read_text()
 for lo,hi in re.findall(r'if\(at>=(0x[0-9a-f]+)&&at<(0x[0-9a-f]+)&&count<=',text):
  supplemental.update(range(int(lo,16),int(hi,16)))
 for at in re.findall(r'if\(at==(0x[0-9a-f]+)&&count==1\)',text):supplemental.add(int(at,16))
 if len(supplemental)!=111 or supplemental&owned:raise SystemExit('Unexpected demo-start mapping or overlap')
 owned.update(supplemental)
missing=[at for at in range(0x2000) if access[at] and at not in owned]
spans=[]
for at in missing:
 if spans and spans[-1][-1]+1==at:spans[-1].append(at)
 else:spans.append([at])
result={'scope':'Observed source bus bytes below 2000 during 1024 CPU-checked demo-start fixtures',
 'limitation':'Subset only; excludes direct resolver writes, text/video, complete ROM coverage and initialization/lifetime proof',
 'source_sha256':hashlib.sha256((root/'build/reference_maincpu.bin').read_bytes()).hexdigest(),
 'trace_sha256':hashlib.sha256(access).hexdigest(),'guarded_supplemental_bytes':len(supplemental),'missing_bytes':len(missing),
 'missing_ranges':[{'start':f'{a[0]:04x}','end_inclusive':f'{a[-1]:04x}','bytes':len(a)} for a in spans]}
(root/'build/demo-start-state.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
