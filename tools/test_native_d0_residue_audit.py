#!/usr/bin/env python3
"""Read-only snapshots and D0 provenance observations on the frozen v6 route.

No changed register equivalence, host CPU writes, timing patches or GUI.
The inherited explicit register-failure collector must still exit1.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import test_native_background_publication as background

ROOT = Path(__file__).resolve().parents[1]
FRAMES = [1405,1448,1466,1467,1469,1470,1500,1501,1503,1506,1530]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform',type=Path,default=ROOT/'build/native-background-publication/target-live-v6-frozen')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-d0-residue-audit/v6-snapshots')
    a = p.parse_args()
    out = a.output.resolve()
    binary, manifest = background.build(a.platform,ROOT/'build/native-background-residency/prepared-v1',out,collect_register_failures=True)
    generated = out/'test_native_render_cadence.c'
    text = generated.read_text()
    text = background.cadence.replace(text,'static unsigned sourcePC(void);','static unsigned sourcePC(void);\nstatic void residueObserve(unsigned,unsigned,unsigned,unsigned);')
    needle = 'static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){'
    text = background.cadence.replace(text,needle,needle+'\n residueObserve(kind,at,width,value);')
    hooks = r'''
static FILE*residueChanges;
static unsigned residueLastD0,residueLastPC,residueLastValid;
static void residueObserve(unsigned kind,unsigned at,unsigned width,unsigned value){
 if(renderPath||frameNumber<1330||frameNumber>1530)return;
 unsigned d0=cpu->dar[0],pc=sourcePC();
 if(!residueLastValid||d0!=residueLastD0){
  if(!residueChanges){residueChanges=outputFile("d0-bus-changes.csv","w");if(!residueChanges)abort();fputs("frame,current_callback_pc,previous_bus_pc,previous_d0,current_d0,kind,address,width,value\n",residueChanges);}
  fprintf(residueChanges,"%u,%u,%u,%u,%u,%u,%u,%u,%u\n",frameNumber,pc,residueLastPC,residueLastD0,d0,kind,at,width,value);
  residueLastD0=d0;residueLastValid=1;
 }
 residueLastPC=pc;
}
static void residueSnapshot(const void*state,size_t bytes){
 static const unsigned wanted[]={1405,1448,1466,1467,1469,1470,1500,1501,1503,1506,1530};
 unsigned match=0;for(unsigned n=0;n<sizeof wanted/sizeof*wanted;n++)match|=frameNumber==wanted[n];if(!match)return;
 char name[128];snprintf(name,sizeof name,"frame-%u.state",frameNumber);FILE*f=outputFile(name,"wb");if(!f||fwrite(state,1,bytes,f)!=bytes)abort();fclose(f);
 snprintf(name,sizeof name,"frame-%u.source",frameNumber);f=outputFile(name,"wb");if(!f)abort();backgroundDumpSource(f,0);fclose(f);
 snprintf(name,sizeof name,"frame-%u.cps",frameNumber);f=outputFile(name,"wb");if(!f||fwrite(cps,1,sizeof cps,f)!=sizeof cps)abort();fclose(f);
 snprintf(name,sizeof name,"frame-%u.registers.bin",frameNumber);f=outputFile(name,"wb");if(!f)abort();for(unsigned n=0;n<21;n++){unsigned value=renderGetRegister(n);for(int b=3;b>=0;b--)if(fputc((value>>(8*b))&255,f)==EOF)abort();}fclose(f);
 snprintf(name,sizeof name,"frame-%u.sram",frameNumber);f=outputFile(name,"wb");if(!f||fwrite(metadataSRAM,1,65536,f)!=65536)abort();fclose(f);
 snprintf(name,sizeof name,"frame-%u.registers.json",frameNumber);f=outputFile(name,"w");if(!f)abort();fprintf(f,"{\"frame\":%u,\"cycles\":%d,\"registers\":[",frameNumber,cpu->cycles);for(unsigned n=0;n<21;n++)fprintf(f,"%s%u",n?",":"",renderGetRegister(n));fputs("]}\n",f);fclose(f);
}
'''
    text = background.cadence.replace(text,'int main(',hooks+'\nint main(')
    text = background.cadence.replace(text,'   if(!path){for(unsigned n=0;n<65536;n++)sourceRAM[n]=ram[n^1];','   if(!path){residueSnapshot(states[0],stateSize);for(unsigned n=0;n<65536;n++)sourceRAM[n]=ram[n^1];')
    text = background.cadence.replace(text,' backgroundFinish();',' if(residueChanges)fclose(residueChanges);\n backgroundFinish();')
    generated.write_text(text)
    subprocess.run(manifest['command'],cwd=ROOT,check=True)
    manifest.update(generated_sha256=background.sha(generated),binary_sha256=background.sha(binary))
    manifest['inputs'][str(Path(__file__).resolve())] = background.sha(Path(__file__))
    core = Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    with (out/'observer.log').open('w') as log:
        result = subprocess.run([str(binary),str(core),'1800','--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
    manifest['run'] = {'exit_code':result.returncode,'frames':1800,'core_sha256':background.sha(core),'log_sha256':background.sha(out/'observer.log')}
    (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    assert result.returncode == 1, 'No source equivalence was relaxed; expected register diagnostic exit1'
    prior = ROOT/'build/native-background-publication/observer-live-v6-diagnostic'
    for name in ['register-failures.csv','background-summary.json','render-summary.json','frames.csv']:
        assert (out/name).read_bytes() == (prior/name).read_bytes(), ('Read-only audit changed route evidence',name)
    evidence = {'scope':__doc__,'frames':FRAMES,'source_state_bytes':(out/f'frame-{FRAMES[0]}.state').stat().st_size,
                'register_failures_unchanged':11,'source_equivalence_admitted':False,
                'files':{str(path.relative_to(out)):background.sha(path) for path in sorted(out.glob('frame-*'))}}
    (out/'snapshot-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2))


if __name__ == '__main__':
    main()
