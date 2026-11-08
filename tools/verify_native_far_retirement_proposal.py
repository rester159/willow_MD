#!/usr/bin/env python3
"""Sanitized ownership-model proof for a proposed zero-BSS retirement phase."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 test=ROOT/'tools/test_native_far_retirement_proposal.c';cache=ROOT/'src/willow_native_far_cache.h';blob=ROOT/'build/native-background-far-stream/prepared-v1/far-stream.bin';cat=ROOT/'build/native-background-catalog/forest-277-v1/catalog.bin'
 pins={str(q):sha(q) for q in [Path(__file__).resolve(),test,cache,blob,cat,ROOT/'tools/native_render_background.inc']}
 cmd=['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),str(test),'-o',str(out/'test')];subprocess.run(cmd,check=True)
 r=subprocess.run([str(out/'test'),str(blob),str(cat)],check=True,capture_output=True,text=True);result=json.loads(r.stdout)
 for q in [test,cache,Path(__file__).resolve()]:shutil.copy2(q,out/q.name)
 reports=[]
 for relative in ['build/native-forest-extension/forest-v2/view130-pixel-demand.json','build/native-forest-extension/forest-v2/boundary-far-pixel-demand.json']:
  q=ROOT/relative;pins[str(q)]=sha(q);v=json.loads(q.read_text());reports.append({k:v[k] for k in ['views','source_pixels_tested','visible_far_pixels_required','missing_exposed_far']})
 assert reports[0]['visible_far_pixels_required']==0 and reports[1]['visible_far_pixels_required']==1
 assert reports[1]['missing_exposed_far'][0]['pattern']==3201
 for q,h in pins.items():assert sha(Path(q))==h
 e={'scope':__doc__,'result':result,'compile':cmd,'actual_source_demand':reports,'capacity_scope':'The actual first missing key requests16 unique pool patterns; previous view130 has zero visible far pixels, nextview132 has one required canonical pattern. This does not certify physical map ownership or later gameplay capacity. Model also proves165 old live plus16 distinct new cannot fit.','inputs':pins};(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');print(r.stdout)
if __name__=='__main__':main()
