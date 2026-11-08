#!/usr/bin/env python3
"""Validate the isolated color capture and identify the cold handoff palette."""
from pathlib import Path
import hashlib,json
path=Path('build/arcade-trace/player-colors/frames.bin');data=path.read_bytes()
stride=4+1026+128+6144
assert len(data)==1151*stride
old=Path('build/arcade-trace/camera-native/frames.bin').read_bytes()
for i in range(1151):assert int.from_bytes(data[i*stride:i*stride+4],'big')==650+i
for i in range(971):
 a=data[i*stride:(i+1)*stride];b=old[i*17670:(i+1)*17670]
 assert a[:1030]==b[:1030],('globals',650+i)
 assert a[1030:1158]==b[1158:1286],('player',650+i)
p=Path('build/reference_maincpu.bin').read_bytes()
full=b''.join((int.from_bytes(p[n:n+2],'big')|0xf000).to_bytes(2,'big') for n in range(0xe5080,0xe50c0,2))
# Establish the actual stable period rather than assuming the whole intro is bright.
for f in range(725,1621):assert data[(f-650)*stride+1158:(f-650)*stride+1222]==full,f
transitions=[];last=None
for f in range(691,726):
 at=(f-650)*stride+1158;brightness=data[at]>>4
 if brightness!=last:transitions.append([f,brightness]);last=brightness
report={'sha256':hashlib.sha256(data).hexdigest(),'record_bytes':stride,'records':1151,'first_frame':650,'last_frame':1800,
 'matching_existing_global_and_player_records':971,'full_brightness_player_palette_frames':[725,1620],
 'initial_brightness_transitions':transitions,'scope':'Original capture verification only. Early fade scheduling is observed, not implemented by this check; later active-input records are reference data, not port validation.'}
Path('build/arcade-player-colors-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
