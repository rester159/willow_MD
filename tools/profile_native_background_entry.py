#!/usr/bin/env python3
"""Read-only entry snapshots from the frozenv6 observer; no admission changes."""
from pathlib import Path
import argparse,hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--frames',type=int,default=1523);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 old=ROOT/'build/native-background-publication/observer-live-v6-diagnostic';m=json.loads((old/'observer-manifest.json').read_text());source=old/'test_native_render_cadence.c';header=old/'frozen-observer-tools/test_native_background_publication.h'
 h=header.read_text();needle='static void backgroundBeforeWork(unsigned at,unsigned width,unsigned value){';assert h.count(needle)==1
 hook='''
 static unsigned captured[3];
 const unsigned wanted[]={1376,1488,1520};
 if(!renderPath&&width==2&&sourcePC()==0x102f76){
  for(unsigned n=0;n<3;n++)if(frameNumber==wanted[n]&&!captured[n]){
   char name[128];snprintf(name,sizeof name,"profile-entry-%u-source.bin",frameNumber);FILE*f=outputFile(name,"wb");if(!f)abort();backgroundDumpSource(f,0);fclose(f);
   snprintf(name,sizeof name,"profile-entry-%u-sram.bin",frameNumber);f=outputFile(name,"wb");if(!f||fwrite(metadataSRAM,1,65536,f)!=65536)abort();fclose(f);captured[n]=1;
  }
 }
'''
 h=h.replace(needle,needle+hook);newHeader=out/'profile-observer.h';newHeader.write_text(h)
 text=source.read_text().replace(str(header),str(newHeader));generated=out/'profile.c';generated.write_text(text)
 command=[str(generated)if x==str(source)else str(out/'profile')if x==str(old/'test-native-render-cadence')else x for x in m['command']];subprocess.run(command,cwd=ROOT,check=True)
 core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
 with(out/'observer.log').open('w')as log:r=subprocess.run([str(out/'profile'),str(core),str(a.frames),'--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=log)
 (out/'profile-manifest.json').write_text(json.dumps({'scope':__doc__,'exit_code':r.returncode,'inherited_diagnostic_register_failure_policy':True,'source_sha256':sha(source),'header_sha256':sha(header),'script_sha256':sha(Path(__file__)),'generated_sha256':sha(generated),'captures':{p.name:sha(p)for p in out.glob('profile-entry-*.bin')}},indent=2)+'\n')
 print('Profile snapshot run finished',r.returncode)
if __name__=='__main__':main()
