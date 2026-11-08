#!/usr/bin/env python3
"""Verify deterministic baseline overlap and the recorded walking input."""
from pathlib import Path
import hashlib,json
path=Path('build/arcade-trace/player-walk/frames.bin');data=path.read_bytes();stride=40198
assert len(data)==114*stride
old=Path('build/arcade-trace/camera-native/frames.bin').read_bytes()
for n in range(114):
 f=1587+n;r=data[n*stride:(n+1)*stride]
 assert int.from_bytes(r[:4],'big')==f
 if f<=1620:assert r[:17670]==old[(f-650)*17670:(f-649)*17670],f
 assert r[4+0x6a]==(1 if 1626<=f<=1665 else 0),(f,'held')
 assert r[4+0x6b]==(1 if 1627<=f<=1666 else 0),(f,'previous')
report={'sha256':hashlib.sha256(data).hexdigest(),'record_bytes':stride,'records':114,'frames':[1587,1700],
 'unchanged_baseline_records':34,'right_held_frames':[1626,1665],'scope':'Original input/player/camera/map/palette/text capture. Rendering fidelity is verified separately.'}
Path('build/arcade-player-walk-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
