#!/usr/bin/env python3
"""Verify initial HUD packing and compare default state against arcade captures."""
from pathlib import Path
import json,re
text=Path('native/intro_probe/src/hud_initial.h').read_text()
maps=[int(v) for v in re.findall(r'\d+',text.split('native_hud_initial_maps[32][240]={')[1].split('};')[0])]
words=[int(v) for v in re.findall(r'\d+',text.split('native_hud_initial_cells[32][576]={')[1].split('};')[0])]
packed=Path('native/intro_probe/res/hud_initial.bin').read_bytes()
source_pixels=Path('build/native-hud-initial-tiles.bin').read_bytes();source_cells=Path('build/native-hud-initial-cells.bin').read_bytes()
assert len(maps)==32*240 and len(words)==32*576
assert b''.join(w.to_bytes(2,'big') for w in words)==source_cells
for n,index in enumerate(maps):assert packed[index*32:index*32+32]==source_pixels[n*32:n*32+32]
checks=0
for phase,frame in ((0,1588),(1,1589)):
 state=source_cells[(10+phase)*1152:(11+phase)*1152]
 capture=Path(f'build/arcade-trace/actor-native/text-{frame}.bin').read_bytes()
 expected=b''.join(capture[col*128+row*4:col*128+row*4+4] for row in (2,3,26,28,29,30) for col in range(48))
 assert state==expected,(phase,frame)
 checks+=len(state)
 pixel_frame=1588 if phase==0 else 1590
 reference=Path(f'build/hud-surface-{pixel_frame}-0.bin').read_bytes()
 assert source_pixels[(10+phase)*7680:(11+phase)*7680]==reference
report={'states':32,'packed_pixel_bytes_checked':len(source_pixels),'retained_word_bytes_checked':len(source_cells),
 'captured_source_bytes_checked':checks,'captured_composed_pixel_bytes_checked':15360,
 'default_dip':5,'scope':'Initial HUD and first clock update from ROM/code; captured source is validation only. Runtime VDP upload/frame budget remains pending.'}
Path('build/native-hud-initial-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
