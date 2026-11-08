#!/usr/bin/env python3
"""Compare ROM-only source mapping to captured CPS video RAM/registers."""
from pathlib import Path
import zipfile,json
from PIL import Image
from cps1_program import assemble_maincpu
from cps1_gfx import assemble_region
from cps1_source_layers import SourceLayers,word
from cps1_captured_background import render
p=assemble_maincpu(Path('willow.zip'))
with zipfile.ZipFile('willow.zip') as z:g=assemble_region(z)
layers=SourceLayers(p,g);root=Path('build/arcade-trace');results=[]
for frame in range(720,1021,30):
 ram=(root/f'intro-vram-{frame}.bin').read_bytes();regs=(root/f'intro-_cps_a_regs-{frame}.bin').read_bytes()
 cameras={2:((word(regs,16)+64)&1023,768-word(regs,18)),3:((word(regs,20)+64)&2047,1792-word(regs,22))}
 source=layers.render(cameras,ram[0x1c000:0x1d000]);reference=render(ram,regs,g).tobytes()
 mismatch=sum(source[i:i+3]!=reference[i:i+3] for i in range(0,len(source),3))
 Image.frombytes('RGB',(384,224),source).save(root/f'rom-background-{frame}.png')
 results.append({'frame':frame,'cameras':cameras,'mismatched_pixels':mismatch,'pixels':384*224})
 print(results[-1])
(root/'source-layer-parity.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(r['mismatched_pixels']==0 for r in results),'Source mapping differs from live layer RAM'
