#!/usr/bin/env python3
"""Fit source lettering and the initial player pose together in Genesis PAL0."""
from pathlib import Path
from collections import Counter
import zipfile,json
from cps1_gfx import assemble_region,decode_tile,genesis_tiles
from cps1_palette import cps_rgb,palette_words
from cps1_program import assemble_maincpu
from native_palette_fit import fit_palette,color_error
with zipfile.ZipFile('willow.zip') as z:gfx=assemble_region(z)
p=assemble_maincpu(Path('willow.zip'))
def colors(layer,group):return [cps_rgb(w|0xf000) for w in palette_words(p,layer,0,group)]
font_colors=colors('scroll1',3);player_colors=colors('base',0);hist=Counter();glyphs=[]
for layout in range(2):
 assert all(p==15 for row in decode_tile(gfx,0x71a6,8,layout) for p in row)
 for code in range(0x7020,0x7060):
  pens=decode_tile(gfx,code,8,layout);glyphs.append(pens)
  hist.update(font_colors[p] for row in pens for p in row if p!=15)
for y in range(3):
 for x in range(3):hist.update(player_colors[p] for row in decode_tile(gfx,0x33+x+y*16,16) for p in row if p!=15)
palette=fit_palette(hist);lookup={c:1+min(range(15),key=lambda i:color_error(c,palette[i])) for c in hist}
out=b''.join(genesis_tiles([[0 if p==15 else lookup[font_colors[p]] for p in row] for row in glyph]) for glyph in glyphs)
words=[0]+[sum((c//34)<<s for c,s in zip(rgb,(1,5,9))) for rgb in palette]
Path('native/intro_probe/res/font.bin').write_bytes(out)
Path('native/intro_probe/src/font.h').write_text('/* Source text and initial player colors share PAL0. */\nstatic const u16 intro_font_palette[16]={'+','.join(map(str,words))+'};\n')
err=sum(n*sum((a-b)**2 for a,b in zip(c,palette[lookup[c]-1])) for c,n in hist.items())
Path('build/intro-font.json').write_text(json.dumps({'genesis_palette':words,'glyphs_per_layout':64,'source_first':0x7020,'shared_player_palette_group':0,'source_unique_colors':len(hist),'source_rgb_rmse':(err/sum(hist.values())/3)**.5},indent=2)+'\n')
