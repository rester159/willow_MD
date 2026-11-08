#!/usr/bin/env python3
"""Observe background source lifetime on the pinned 1800-frame content route.

Composes only read-only host callbacks; neither target nor source is changed.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess

ROOT=Path(__file__).resolve().parents[1]
PIN=ROOT/'build/native-render-cadence/checkpoint-content-v2'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def replace(text,old,new):
 assert text.count(old)==1,(old[:80],text.count(old))
 return text.replace(old,new)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--output',type=Path,default=ROOT/'build/native-background-lifetime/v1')
 p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')
 a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 pin=json.loads((PIN/'checkpoint.json').read_text())['files']
 # Exact observer, target products, and frozen host includes used below.
 names=[n for n in pin if n.startswith(('target/','workspace/')) or n in ['observer/test_native_render_cadence.c','observer/render-oracle-assets.h']]
 for n in names:assert sha(PIN/n)==pin[n],n
 text=(PIN/'observer/test_native_render_cadence.c').read_text()
 text=text.replace(str(ROOT)+'/',str(PIN/'workspace')+'/')
 text=text.replace(str(PIN/'workspace/build/native-render-cadence/observer-content-current'),str(out))
 shutil.copy2(PIN/'observer/render-oracle-assets.h',out/'render-oracle-assets.h')
 # Two generic host headers were not part of the renderer archive; copy and pin.
 for name in ['tools/vendor/libretro.h','src/willow_arcade_player_motion.h']:
  source=ROOT/name;dst=out/'inputs'/name;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dst)
  text=text.replace(str(PIN/'workspace'/name),str(dst))
 text=replace(text,'static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){','#include "'+str(ROOT/'tools/native_background_lifetime_observer.h')+'"\nstatic void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){\n bgTrace(kind,at,width,value);')
 text=replace(text,'  headroomEvent("irq_first_native_store",savedPC,savedSR);','  bgIRQ(savedPC);\n  headroomEvent("irq_first_native_store",savedPC,savedSR);')
 text=replace(text,' headroomEvents=outputFile(', ' bgInit();\n headroomEvents=outputFile(')
 text=replace(text,' renderFinish();',' bgFinish();\n renderFinish();')
 generated=out/'test_background_lifetime.c';generated.write_text(text)
 binary=out/'test-background-lifetime'
 subprocess.run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror',str(generated),'-o',str(binary)],cwd=ROOT,check=True)
 with (out/'observer.log').open('w')as f:
  subprocess.run([str(binary),str(a.core),'1800','--coin-route',str(out)],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
 result={'scope':__doc__,'checkpoint':sha(PIN/'checkpoint.json'),'source_sha256':sha(ROOT/'build/reference_maincpu.bin'),'core_sha256':sha(a.core),
  'tools':{str(q):sha(q) for q in [Path(__file__),ROOT/'tools/native_background_lifetime_observer.h']},
  'outputs':{q.name:sha(q) for q in out.iterdir() if q.is_file() and q.name!='manifest.json'},
  'limitations':'Same Genesis NTSC execution and immutable existing object renderer; no new background rendering, source instructions, scheduling, or CPS 10MHz timing equivalence. Callback PC is instruction-end bus PC, not necessarily instruction start.'}
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
 print((out/'observer.log').read_text().splitlines()[-1])
if __name__=='__main__':main()
