#!/usr/bin/env python3
"""Pack ROM-derived initial HUD outputs; captures are never input here."""
from pathlib import Path
import json
cells=Path('build/native-hud-initial-cells.bin').read_bytes()
pixels=Path('build/native-hud-initial-tiles.bin').read_bytes()
assert len(cells)==16*2*1152 and len(pixels)==16*2*7680
patterns=[bytes(32)];ids={patterns[0]:0};maps=[]
for case in range(32):
 row=[]
 for n in range(240):
  tile=pixels[case*7680+n*32:case*7680+(n+1)*32]
  if tile not in ids:ids[tile]=len(patterns);patterns.append(tile)
  row.append(ids[tile])
 maps.append(row)
cell_words=[[int.from_bytes(cells[i+j:i+j+2],'big') for j in range(0,1152,2)] for i in range(0,len(cells),1152)]
Path('native/intro_probe/res/hud_initial.bin').write_bytes(b''.join(patterns))
Path('native/intro_probe/src/hud_initial.h').write_text('/* Original HUD C and ROM tables; no captured runtime inputs. */\n'
 f'#define NATIVE_HUD_INITIAL_PATTERNS {len(patterns)}\n'
 'static const unsigned short native_hud_initial_maps[32][240]={'+','.join('{'+','.join(map(str,r))+'}' for r in maps)+'};\n'
 'static const unsigned short native_hud_initial_cells[32][576]={'+','.join('{'+','.join(map(str,r))+'}' for r in cell_words)+'};\n')
report={'status':'ROM-derived cold initial HUD preparation data','dip_combinations':16,'states_per_dip':2,
 'patterns':len(patterns),'pattern_bytes':len(patterns)*32,'map_bytes':32*240*2,'retained_cell_bytes':len(cells),
 'default_initial_patterns':len(set(maps[10])-{0}),'default_first_clock_patterns':len(set(maps[11])-{0}),
 'scope':'Cold single-player Stage 0 and first clock update from source logic. Other initial states and later HUD changes require live updates.'}
Path('build/native-hud-initial-assets.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
