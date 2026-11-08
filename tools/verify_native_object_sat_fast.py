#!/usr/bin/env python3
"""Run the unchanged independent source-pixel oracle against the fast consumer."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'tools/verify_native_object_sat_bridge.py'
text=source.read_text()
changes={"OUT/'object-sat-test.dylib'":"OUT/'object-sat-fast-test.dylib'",'tools/test_native_object_sat_bridge.c':'tools/test_native_object_sat_fast.c','willowObjectSatTestBuild':'willowObjectSatFastTestBuild','willowObjectSatTestReserve':'willowObjectSatFastTestReserve','object-sat-evidence.json':'object-sat-fast-evidence.json'}
for old,new in changes.items():
 assert old in text,old
 text=text.replace(old,new)
# Only C adapter names/artifact paths change; pixel-first reference() and the
# original ROM decoder, captured banks, synthetic hardware cases are unchanged.
exec(compile(text,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
p=ROOT/'build/cartridge-gap-audit/object-sat-fast-evidence.json';d=json.loads(p.read_text());d['independent_oracle_sha256']=hashlib.sha256(source.read_bytes()).hexdigest();d['fast_header_sha256']=hashlib.sha256((ROOT/'src/willow_native_object_sat_fast.h').read_bytes()).hexdigest();p.write_text(json.dumps(d,indent=2)+'\n')
