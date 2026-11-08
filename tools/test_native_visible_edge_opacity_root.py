#!/usr/bin/env python3
"""Sanitized actual root partial-edge/byte-overlap/row-clear functions."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(s,name):
 for m in re.finditer(r'^static[^\n]*\b'+name+r'\(',s,re.M):
  begin=s.index('{',m.start())
  if ';'in s[m.start():begin]:continue
  level=1;i=begin+1
  while level:
   if s[i]=='{':level+=1
   elif s[i]=='}':level-=1
   i+=1
  return s[m.start():i]
 raise AssertionError(name)
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT/'tools/native_render_background.inc');p.add_argument('--output',type=Path,default=ROOT/'build/native-visible-opacity/actual-edges-v1');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);source=a.source.resolve();s=source.read_text();names=['bgRef','bgFirstX','bgFirstY','bgRows','bgSet','bgNearOpacityRef','bgNearVisibleRowFull','bgFarRowBlank','bgFarBlank','bgFarNeeded','bgResolveRef','bgResolveCell','bgResolveRow'];funcs='\n'.join(extract(s,n)for n in names);(out/'actual-functions.h').write_text(funcs+'\n')
 test=ROOT/'tools/test_native_visible_edge_opacity_root.c';exe=out/'test';cmd=['clang','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'src'),'-I'+str(out),str(test),'-o',str(exe)];subprocess.run(cmd,check=True);fixtures=[ROOT/('build/native-visible-opacity/edges-v1/source-masks-view'+str(n)+'.bin')for n in(120,130,132)];r=subprocess.run([str(exe),*[str(f)for f in fixtures]],check=True,capture_output=True,text=True);result=json.loads(r.stdout)
 evidence={'scope':__doc__,'source_sha256':sha(source),'extracted_functions':names,'function_bodies_sha256':hashlib.sha256(funcs.encode()).hexdigest(),'compile':cmd,'result':result,'inputs':{str(p):sha(p)for p in[Path(__file__),test,source,ROOT/'src/willow_native_background_key_cache.h',ROOT/'src/willow_native_plane_surface.h',*fixtures]}};(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(r.stdout)
if __name__=='__main__':main()
