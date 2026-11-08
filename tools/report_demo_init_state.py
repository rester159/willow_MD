#!/usr/bin/env python3
"""Identify observed demo global/status storage gaps, not full bus coverage."""
from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parents[1]
trace=root/'build/native-demo-init-bus-access.bin'
access=trace.read_bytes()
if len(access)!=65536 or any(x>3 for x in access):
    raise SystemExit('Expected a complete read/write bitset from the verified native adapter')
header=root/'src/willow_native_boot_globals.h'
boot=header.read_text()
owned=set()
for at,size,offset in re.findall(r'\{(0x[0-9a-f]+),(\d+),(\d+)\}',boot):
    owned.update(range(int(at,16),int(at,16)+int(size)))
# Existing guarded opening/pool owners. These need coherent aliases on a demo
# bus; their presence here does not claim introQueueWord already routes them.
owned.update((0x8344,0x8345,0x83c3,0x83cb))
# Focus on the player's saved/live status region, option fields and cycle byte.
# Pool records/indices, palettes and arbitrary fixture RAM pointers are excluded.
candidates=set(range(0x8287,0x828e))|set(range(0x82c2,0x8402))|{0xf9a2}
supplemental=set()
state_header=root/'src/willow_native_demo_state.h'
main=(root/'native/intro_probe/src/main.c').read_text()
if state_header.exists() and 'willowNativeDemoStateResolve(SOURCE_DEMO_STATE,at,bytes)' in main:
    state=state_header.read_text()
    for lo,hi in re.findall(r'if\(at>=(0x[0-9a-f]+)&&at<(0x[0-9a-f]+)&&count<=',state):
        supplemental.update(range(int(lo,16),int(hi,16)))
    for at in re.findall(r'if\(at==(0x[0-9a-f]+)&&count==1\)',state):
        supplemental.add(int(at,16))
    if len(supplemental)!=321 or supplemental&owned:
        raise SystemExit('Supplemental demo ownership changed or overlaps existing bytes')
    owned.update(supplemental)
observed=sorted(at for at in candidates if access[at])
missing=[at for at in observed if at not in owned]
spans=[]
for at in missing:
    if not spans or at!=spans[-1][-1]+1:spans.append([at])
    else:spans[-1].append(at)
report={
 'scope':'1024 original-CPU-checked demo-init fixtures; global/player-status subset only',
 'limitation':'Observed byte lower bound; excludes pool/data/palette capability, cross-owner words, initialization and lifetime',
 'source_sha256':hashlib.sha256((root/'build/reference_maincpu.bin').read_bytes()).hexdigest(),
 'trace_sha256':hashlib.sha256(access).hexdigest(),
 'boot_mapping_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),
 'observed_subset_bytes':len(observed),'existing_owner_bytes':len(observed)-len(missing)-len(set(observed)&supplemental),
 'guarded_supplemental_bytes':len(set(observed)&supplemental),
 'missing_bytes':len(missing),
 'missing_ranges':[{'start':f'{r[0]:04x}','end_inclusive':f'{r[-1]:04x}','bytes':len(r)} for r in spans],
 'alias_requirements':['8288 word straddles existing starting-health byte and supplemental 8289',
                       '8342 word straddles saved boot variant byte 8343',
                       '8344 word uses guarded stage storage',
                       '83c2 word straddles guarded byte 83c3',
                       '83ca word straddles pool auxiliary byte 83cb',
                       'f9a2 word straddles existing starting-lives byte f9a3']}
(root/'build/native-demo-init-state.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
