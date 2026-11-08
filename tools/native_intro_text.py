#!/usr/bin/env python3
from pathlib import Path
import json
from cps1_program import assemble_maincpu
from cps1_text_task import decode
p=assemble_maincpu(Path('willow.zip'))
messages=[decode(p,46,0x1302),decode(p,47,0x1303),decode(p,49,0x1305),decode(p,0x419,0x0306),decode(p,0x640,0x1412),decode(p,0x641,0x1413)]
lines=['/* Source task 78F0 / 7994; do not replace with inferred prose. */', '#ifndef WILLOW_GENERATED_INTRO_TEXT_H', '#define WILLOW_GENERATED_INTRO_TEXT_H', '#include "../src/willow_arcade_text.h"']
for m in messages:
 lines.append('static const unsigned char intro_text_%d[]={%s};'%(m['message'],','.join(map(str,m['stream']))))
for m in messages:
 lines.append('static const unsigned short intro_attrs_%d[]={%s};'%(m['message'],','.join(map(str,m['attributes']))))
 lines.append('static const WillowArcadeMessage intro_message_%d={intro_text_%d,intro_attrs_%d,0x%xUL,%d,%d};'%(m['message'],m['message'],m['message'],m['destination'],m['delay'],m['rows']))
lines.append('#endif')
Path('build/arcade_intro_text.h').write_text('\n'.join(lines)+'\n')
Path('build/arcade_intro_text.json').write_text(json.dumps([{**m,'stream':(m['stream'].hex() if m['handler']==6 else m['stream'].decode('ascii'))} for m in messages],indent=2)+'\n')
