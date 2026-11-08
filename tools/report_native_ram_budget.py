"""Inventory source-state storage for a prospective RAM-layout change; not proof of fit."""
import argparse
import re
import subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('elf',type=Path)
a=p.parse_args()
symbols={}
for line in subprocess.check_output(['m68k-elf-nm','-S',str(a.elf)],text=True).splitlines():
    parts=line.split()
    if len(parts)==4:symbols[parts[3]]=int(parts[1],16)
names=['debugStageObjects','debugPaletteControllers','debugPlayerRecord','debugScroll2Record','debugScroll3Record','taskRecords','taskCommands','soundCommands','sourceBootGlobals','sourceHighScores']
rows=[(name,symbols[name]) for name in names]
specs=[(int(first,16),int(capacity),int(stride)) for first,capacity,stride in re.findall(r'\{(0x[0-9a-f]+),(\d+),(\d+)\}',Path('src/willow_arcade_actor_pool.h').read_text())]
assert len(specs)==9
rows += [('eight dynamically allocated actor payloads',sum(n*stride for _,n,stride in specs[:8]))]
size=int(re.search(r'bytes\[(\d+)\]',Path('src/willow_native_class12_visibility.h').read_text()).group(1))
rows += [('dynamically allocated class-12 visibility',size)]
for name,size in rows:print(f'{size:6,d}  {name}')
total=sum(size for _,size in rows)
print(f'{total:6,d}  candidate main-RAM source storage, before redesign')
print(f'{6144+2+16384:6,d}  current SRAM video storage (object banks/display, selector, Scroll1)')
print(f'{total-6144-2-16384:6,d}  gross difference, NOT usable free memory')
print('Excluded: class-18 payload inside a startup lookup union (no automatic reclamation), pool indices/descriptors, task control/runtime, allocator/alignment overhead, and other SRAM execution/supplemental owners.')
print('A flat SRAM source view would require moving video/runtime owners, rebinding every direct record user, and reconciling compressed pool/high-score representations. This inventory does not prove that layout or full campaign storage fits.')
