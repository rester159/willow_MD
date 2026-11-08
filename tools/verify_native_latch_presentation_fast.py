#!/usr/bin/env python3
"""Reuse the root native latch fixture with only its consumer entry overridden."""
from pathlib import Path
import argparse,hashlib,json,re,runpy,sys
from native_object_sat_resident_index import emit_resident_header
import build_vdp_presentation_probe as probe
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/native-latch-presentation/fast-geometry');p.add_argument('--optimization',choices=['O2','O3','Os'],default='O2');p.add_argument('--native-budget',action='store_true');p.add_argument('--sequential-reader',action='store_true');p.add_argument('--scan-zero-runs',action='store_true');p.add_argument('--resident-index',action='store_true');p.add_argument('--fixture',type=Path);p.add_argument('--asset-binding',type=Path);args=p.parse_args()
if args.resident_index:args.scan_zero_runs=True
if args.scan_zero_runs:args.sequential_reader=True
entry=ROOT/('tools/native_latch_presentation_fast_indexed.c' if args.resident_index else 'tools/native_latch_presentation_fast_scan.c' if args.scan_zero_runs else 'tools/native_latch_presentation_fast_serial.c' if args.sequential_reader else 'tools/native_latch_presentation_fast_m68k.c' if args.native_budget else 'tools/native_latch_presentation_fast.c');original_build=probe.build
if args.sequential_reader and not args.native_budget:p.error('--sequential-reader currently requires --native-budget')
def selected_build(inputs,out,**kwargs):
 assert kwargs.get('native_source')==ROOT/'tools/native_latch_presentation.c'
 kwargs['native_source']=entry
 if args.resident_index:
  destination=Path(kwargs.get('native_include') or out)
  source=(destination/'latch_fixture.h').read_text()
  match=re.search(r'fixtureAssets\[\]=\{(.*?)\};',source,re.S);assert match
  triples=[tuple(map(int,m)) for m in re.findall(r'\{(\d+),(\d+),(\d+)\}',match.group(1))]
  assets={key:(attribute,status) for key,attribute,status in triples};assert len(assets)==len(triples)
  header,index=emit_resident_header(assets,77)
  (destination/'resident_fixture.h').write_text(header)
  (destination/'resident-index.json').write_text(json.dumps(index,indent=2)+'\n')
 return original_build(inputs,out,**kwargs)
probe.build=selected_build
sys.argv=[str(ROOT/'tools/verify_native_latch_presentation.py'),'--deferred-budget','--output',str(args.output),'--optimization',args.optimization]
if args.fixture:sys.argv+=['--fixture',str(args.fixture)]
if args.asset_binding:sys.argv+=['--asset-binding',str(args.asset_binding)]
runpy.run_path(str(ROOT/'tools/verify_native_latch_presentation.py'),run_name='__main__')
report_path=args.output.resolve()/'evidence.json';report=json.loads(report_path.read_text());report['consumer_variant']='optional word-sized original geometry; unchanged root inputs/oracle/VDP publication';report['native_entry']=str(entry);report['native_budget']=args.native_budget;report['sequential_reader']=args.sequential_reader;report['scan_zero_runs']=args.scan_zero_runs;report['resident_index']=args.resident_index
for path in (entry,ROOT/'src/willow_native_object_sat_fast.h',ROOT/'src/willow_native_object_sat_budget_m68k.h',ROOT/'src/willow_native_object_latch_sequential.h',ROOT/'src/willow_native_object_latch_next.h',ROOT/'src/willow_native_object_sat_resident_fast.h',ROOT/'tools/native_object_sat_resident_index.py',ROOT/'tools/native_latch_presentation_fast_serial.c',Path(__file__).resolve()):report['hashes'][str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
if args.resident_index:
 for path in (args.output/'resident_fixture.h',args.output/'resident-index.json'):report['hashes'][str(path.resolve())]=hashlib.sha256(path.read_bytes()).hexdigest()
report_path.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['timing'],indent=2))
