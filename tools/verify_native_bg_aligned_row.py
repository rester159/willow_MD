#!/usr/bin/env python3
"""Actual68000 fused-row parity, source fixtures and isolated predicate timing."""
import argparse,csv,hashlib,json,shutil,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--old',type=Path,default=ROOT/'build/native-render-cadence/far-stream-v3');p.add_argument('--new',type=Path,default=ROOT/'build/native-render-cadence/far-stream-v4');p.add_argument('--output',type=Path,required=True);p.add_argument('--core',type=Path,default=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib');a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True);old,new=a.old.resolve(),a.new.resolve();om=json.loads((old/'manifest.json').read_text());nm=json.loads((new/'manifest.json').read_text());assert om['symbols']['bg']==nm['symbols']['bg'];pins={}
 def pin(q):pins[str(q.resolve())]=sha(q);return q.read_bytes()
 for name,t in [('old',old),('new',new)]:(out/(name+'.bin')).write_bytes(pin(t/'render.bin'));pin(t/'manifest.json')
 (out/'resources.bin').write_bytes(pin(new/'background-resources.bin'))
 fields={'BG_SIZE':'sizeof(bg)','VIEW':'offsetof(__typeof__(bg),view)','COUNTS':'offsetof(__typeof__(bg),rowCounts)','RINGS':'offsetof(__typeof__(bg),rings)','KEYS':'offsetof(__typeof__(bg),keys)','NEARLOW':'offsetof(WillowNativeBackgroundKeyCache,nearLow)','NEARHIGH':'offsetof(WillowNativeBackgroundKeyCache,nearHigh)'}
 flags=[x for x in nm['background_far_stream']['compile_commands'][0] if x.startswith('-I') or x.startswith('-D')]
 (out/'layout.c').write_text('#include "'+str(ROOT/'tools/native_render_cadence.c')+'"\nconst uint32_t auditLayout[] __attribute__((section(".audit_layout")))={'+','.join(fields.values())+'};\n')
 subprocess.run(['m68k-elf-gcc','-m68000','-O2','-ffreestanding','-fno-builtin',*flags,'-c',str(out/'layout.c'),'-o',str(out/'layout.o')],check=True);subprocess.run(['m68k-elf-objcopy','-O','binary','-j','.audit_layout',str(out/'layout.o'),str(out/'layout.bin')],check=True)
 offsets=dict(zip(fields,struct.unpack('>7I',(out/'layout.bin').read_bytes())));assert offsets['BG_SIZE']==9020
 run=ROOT/'build/native-forest-extension/columns-v1-provenance-v2';sourceTarget=ROOT/'build/native-forest-extension/target-columns-v1-frozen';sm=json.loads(pin(sourceTarget/'manifest.json'));pub=list(csv.DictReader((run/'background-publications.csv').open()));pin(run/'background-publications.csv');pin(run/'background-source-contexts.bin');selections=[next(r for r in pub if r['sample']=='433'),next(r for r in pub if r['s2x']=='122'),next(r for r in pub if r['s2x']=='130')]
 with (run/'background-source-contexts.bin').open('rb') as f:
  raws=[]
  for row in selections:f.seek(int(row['sample'])*123392);raws.append(f.read(123392))
 raws.append(pin(run/'rejection-004-source.bin'));sourceBG=sm['symbols']['bg']&65535
 for n,raw in enumerate(raws):(out/f'state{n}.bin').write_bytes(raw[0xe200+sourceBG:0xe200+sourceBG+offsets['BG_SIZE']])
 defines={**offsets,'BG_BASE':nm['symbols']['bg'],'OLD_PC':om['symbols']['bgFarBlank'],'NEW_PC':nm['symbols']['bgFarBlank']};(out/'aligned-fixture.h').write_text(''.join(f'#define {k} 0x{v:x}u\n' for k,v in defines.items()))
 driver=ROOT/'tools/test_native_bg_aligned_row.c';helper=ROOT/'tools/test_native_render_cache_closed.c'
 for q in [Path(__file__).resolve(),driver,helper,a.core,ROOT/'tools/native_render_background.inc',ROOT/'tools/native_render_cadence.c']:pin(q)
 command=['cc','-O2','-g','-std=c11','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'tools'),'-I'+str(out),str(driver),'-o',str(out/'test')];subprocess.run(command,check=True)
 with (out/'test.log').open('w') as f:subprocess.run([str(out/'test'),str(a.core),str(out)],stdout=f,stderr=subprocess.STDOUT,check=True)
 rows=list(csv.DictReader((out/'calls.csv').open()));assert len(rows)==2180;fixtures=rows[:4];assert [int(r['result']) for r in fixtures]==[1,1,1,0]
 for r in fixtures:r['old_ms']=int(r['old_master'])/53693.175;r['new_ms']=int(r['new_master'])/53693.175
 assert all(sha(Path(q))==h for q,h in pins.items());e={'scope':__doc__,'cases':len(rows),'source_fixtures':fixtures,'source_samples':selections,'layout':defines,'compile':command,'result':(out/'test.log').read_text(),'limitations':'Closed calls use source-captured native cache bytes and explicit diagnostic rotations/refusals. CPU refresh insertion is disabled to isolate instruction costs. No pending-map/source scheduler, active VDP or frame/FPS admission. Existing host72M edge proof is independent.','inputs':pins};(out/'evidence.json').write_text(json.dumps(e,indent=2)+'\n');shutil.copy2(driver,out/driver.name);shutil.copy2(Path(__file__),out/Path(__file__).name);print(e['result']);print(json.dumps(fixtures,indent=2))
if __name__=='__main__':main()
