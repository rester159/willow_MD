#!/usr/bin/env python3
"""Check frame identity and overlap with independently captured source traces."""
from pathlib import Path
import hashlib,json
p=Path('build/arcade-trace/scroll3-walk/frames.bin');data=p.read_bytes();stride=1286
assert len(data)==1051*stride
maps=Path('build/arcade-trace/scroll3-walk/maps.bin').read_bytes()
assert len(maps)==1051*16384
intro=Path('build/arcade-trace/camera-native/frames.bin').read_bytes()
walk=Path('build/arcade-trace/player-walk/frames.bin').read_bytes()
for n in range(1051):
 frame=650+n;r=data[n*stride:(n+1)*stride]
 assert int.from_bytes(r[:4],'big')==frame
 if frame<=1620:assert r[:1158]==intro[n*17670:n*17670+1158],frame
 if frame>=1587:
  off=(frame-1587)*40198
  assert r[:1158]==walk[off:off+1158],frame
report={'sha256':hashlib.sha256(data).hexdigest(),'map_sha256':hashlib.sha256(maps).hexdigest(),'map_bytes_per_frame':16384,'record_bytes':stride,'records':1051,
 'frames':[650,1700],'intro_overlap':971,'walk_overlap':114,
 'scope':'Original globals and both background controller records. Full controller and map replay is checked by test_arcade_scroll3_trace; native rendered output remains unverified.'}
Path('build/arcade-scroll3-trace-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
