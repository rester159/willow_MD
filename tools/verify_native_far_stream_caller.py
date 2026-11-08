#!/usr/bin/env python3
"""Sanitized actual-root far-stream caller lifecycle checks; no target edits."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(s,name):
 for m in re.finditer(r'^static[^\n]*\b'+name+r'\(',s,re.M):
  begin=s.index('{',m.start())
  if ';' in s[m.start():begin]:continue
  level=1;i=begin+1
  while level:
   if s[i]=='{':level+=1
   elif s[i]=='}':level-=1
   i+=1
  return s[m.start():i]
 raise AssertionError(name)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=ROOT/'tools/native_render_background.inc');p.add_argument('--output',type=Path,default=ROOT/'build/native-far-stream-caller/v1');a=p.parse_args()
 out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 source=a.source.resolve();raw=source.read_text();source_hash=sha(source)
 names=['bgFirstX','bgFirstY','bgRows','bgFarProtectWord','bgSet','bgFarClearRow','bgResolveRef','bgFarContext','bgFarRetry','bgFarFinish','bgFarPublishCurrent','bgFarPublished']
 funcs='\n\n'.join(extract(raw,n) for n in names);(out/'actual-caller-functions.h').write_text(funcs+'\n')
 test=ROOT/'tools/test_native_far_stream_caller.c';cache=ROOT/'src/willow_native_far_cache.h';shutil.copy2(cache,out/cache.name);cache_hash=sha(cache)
 cmd=['clang','-O2','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-DTEST_HAS_ROW_PROOF='+str(int('BG_ROW_PROOF_ARGUMENT' in raw)),'-I'+str(out),'-I'+str(ROOT/'src'),str(test),'-o',str(out/'test')]
 subprocess.run(cmd,check=True)
 resource=ROOT/'build/native-background-far-stream/prepared-v1/far-stream.bin';catalog=ROOT/'build/native-background-catalog/forest-277-v1/catalog.bin'
 r=subprocess.run([str(out/'test'),str(resource),str(catalog)],check=True,capture_output=True,text=True);result=json.loads(r.stdout)
 assert sha(source)==source_hash and sha(cache)==cache_hash
 shutil.copy2(source,out/'native_render_background.inc');shutil.copy2(test,out/test.name)
 report={'scope':__doc__,'result':result,'source_sha256':source_hash,'cache_sha256':cache_hash,'extracted_functions':names,'function_bodies_sha256':hashlib.sha256(funcs.encode()).hexdigest(),'compile':cmd,
 'explicit_test_inputs':'bgFarNeeded predicate and post-VDP physical drain certificate. Existing source-opacity/native VDP gates are separate.',
 'inputs':{str(q):sha(q) for q in [Path(__file__).resolve(),test,source,cache,ROOT/'src/willow_native_plane_surface.h',resource,catalog]}}
 (out/'evidence.json').write_text(json.dumps(report,indent=2)+'\n');print(r.stdout)
if __name__=='__main__':main()
