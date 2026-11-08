#!/usr/bin/env python3
"""Run host-side gameplay checks without linking SGDK platform headers."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
(root/'build').mkdir(exist_ok=True)
source=(root/'build/willow_collision.h').read_text().replace('#include <genesis.h>','')
(root/'build/terrain_collision_fixture.h').write_text(source)
for name in ('willow_magic','willow_terrain','companion','sorsha_combat'):
    output=root/'build'/('test_'+name)
    subprocess.run(['clang','-O2',str(root/'tools'/('test_'+name+'.c')),'-o',str(output)],check=True)
    subprocess.run([str(output)],check=True)
