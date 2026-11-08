#!/usr/bin/env python3
"""Closed actual68000 original source writer versus native column hooks."""
from pathlib import Path
import argparse,csv,hashlib,json,re,subprocess
from build_native_background_columns import ROOT,LEAVES,BULKS,build
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def route_fixture(out):
 trace=ROOT/'build/native-background-lifetime/v1'
 selected=[r for r in csv.DictReader((trace/'background-events.csv').open()) if r['kind']=='3' and r['manager']==r['scene']=='2' and r['callback_pc'] in ('437ac','437b0','437b2','43df8','43dfc','43dfe')]
 assert len(selected)==768
 pairs=[]
 for i in range(0,len(selected),3):
  raw,adjust,attr=selected[i:i+3];layer=2 if raw['callback_pc']=='437ac' else 3
  assert [int(x['address'],16) for x in [raw,adjust,attr]]==[int(raw['address'],16)]*2+[int(raw['address'],16)+2]
  pairs.append([int(raw['frame']),layer,int(raw['address'],16),int(raw['a1'],16)-2,int(raw['d0'],16),int(raw['previous'],16),int(attr['previous'],16),int(adjust['value'],16),int(attr['value'],16)])
 groups=[];i=0
 while i<len(pairs):
  first=pairs[i];n=(first[4]&65535)+1;chunk=pairs[i:i+n];assert len(chunk)==n
  for j,r in enumerate(chunk):assert r[:2]==first[:2] and r[2]==first[2]+j*4 and r[3]==first[3]+j*4 and r[4]==first[4]-j
  groups.append([first[0],first[1],first[2],first[3],first[4],i,n]);i+=n
 assert len(set(r[0] for r in pairs))==7
 baseline=(trace/'background-active-maps.bin').read_bytes()[:32768];(out/'route-baseline.bin').write_bytes(baseline)
 header='static const unsigned routePairs[][9]={'+','.join('{'+','.join(hex(v)for v in r)+'}'for r in pairs)+'};\n'
 header+='static const unsigned routeGroups[][7]={'+','.join('{'+','.join(hex(v)for v in r)+'}'for r in groups)+'};\n'
 (out/'writer-route.h').write_text(header)
 return dict(pairs=len(pairs),changed=sum(r[5:7]!=r[7:9]for r in pairs),source_leaf_calls=len(groups),epochs=7,trace_sha256=sha(trace/'background-events.csv'),baseline_sha256=sha(trace/'background-active-maps.bin'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-background-columns/current');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib';results=[]
 tracked=[ROOT/'tools/test_native_background_columns.c',ROOT/'tools/native_background_writer_irq_probe.S',ROOT/'tools/build_native_background_columns.py',ROOT/'tools/native_background_columns_hooks.S',ROOT/'tools/native_background_columns_abi.c',ROOT/'src/willow_native_background_columns.h',ROOT/'tools/test_native_render_cache_closed.c',ROOT/'tools/vendor/libretro.h',Path(__file__)]
 before={str(p):sha(p)for p in tracked}
 for bus in range(2):
  target=out/('native'if bus else'original');m=build(target,0x20f000,0x100100,0x204000 if bus else 0x918000,0x208000 if bus else 0x914000)
  route=route_fixture(target)
  subprocess.run(['m68k-elf-gcc','-m68000','-c',str(ROOT/'tools/native_background_writer_irq_probe.S'),'-o',str(target/'irq.o')],check=True)
  subprocess.run(['m68k-elf-ld','-Ttext=0x1f0000','-e','willowBackgroundWriterIRQProbe',str(target/'irq.o'),'-o',str(target/'irq.elf')],check=True)
  subprocess.run(['m68k-elf-objcopy','-O','binary',str(target/'irq.elf'),str(target/'irq.bin')],check=True)
  s=m['symbols'];header=f'#define BEGIN {s["willowBackgroundColumnsBeginEpoch"]:#x}\n#define COMMIT {s["willowBackgroundColumnsCommitEpoch"]:#x}\n'
  header+='static const unsigned leafPC[]={'+','.join(hex(at)for at,_,_ in LEAVES)+'};\n'
  header+='static const unsigned leafStores[]={'+','.join(hex(s[name+'SourceStores'])for _,name,_ in LEAVES)+'};\n'
  header+='static const uint8_t leafOriginal[][8]={'+','.join('{'+','.join(hex(v)for v in bytes.fromhex(raw))+'}'for _,_,raw in LEAVES)+'};\n'
  patches=[next(p for p in m['patches']if p['address']==at)for at,_,_ in LEAVES]
  header+='static const uint8_t leafPatch[][8]={'+','.join('{'+','.join(hex(v)for v in bytes.fromhex(p['replacement']))+'}'for p in patches)+'};\n'
  header+='static const unsigned bulkPC[]={'+','.join(hex(at)for at,_,_,_ in BULKS)+'};\n'
  header+='static const uint8_t bulkOriginal[][6]={'+','.join('{'+','.join(hex(v)for v in bytes.fromhex(raw+tail)[:6])+'}'for _,_,raw,tail in BULKS)+'};\n'
  bp=[next(p for p in m['patches']if p['address']==at)for at,_,_,_ in BULKS]
  header+='static const uint8_t bulkPatch[][6]={'+','.join('{'+','.join(hex(v)for v in bytes.fromhex(p['replacement']))+'}'for p in bp)+'};\n'
  (target/'writer-fixture.h').write_text(header)
  subprocess.run(['clang','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(target),str(ROOT/'tools/test_native_background_columns.c'),'-o',str(target/'test')],check=True)
  with (target/'calls.csv').open('w')as f,(target/'test.log').open('w')as log:subprocess.run([str(target/'test'),str(core),str(target),str(bus)],cwd=ROOT,stdout=f,stderr=log,check=True)
  rows=list(csv.DictReader((target/'calls.csv').open()));results.append(dict(bus=bus,functional_cases=int(re.search(r'PASS (\d+)',(target/'test.log').read_text())[1]),timed_calls=len(rows),irq_injections=int(re.search(r'IRQS (\d+)',(target/'test.log').read_text())[1]),instruction_gap_checks=int(re.search(r'GAPS (\d+)',(target/'test.log').read_text())[1]),max_added_ms=max(int(r['added_master'])for r in rows)/53693.175,route=[dict(frame=int(r['ccr']),pairs=int(r['count']),pending_columns=int(r['entries']),added_ms=int(r['added_master'])/53693.175)for r in rows if r['type']=='route']))
 assert before=={str(p):sha(p)for p in tracked},'Inputs changed while test was running'
 result=dict(scope=__doc__,results=results,route_inputs=route,core_sha256=sha(core),source_inputs_stable=True,files=before)
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
