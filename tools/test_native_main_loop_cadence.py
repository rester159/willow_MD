#!/usr/bin/env python3
"""Read-only original main-body/sleep and Scroll2/3 writer timing observations.

Target execution is unchanged. This wrapper retains every existing register
failure and must reproduce the prior observer outputs byte for byte.
"""
from pathlib import Path
import argparse
import json
import subprocess
import test_native_background_publication as background

ROOT = Path(__file__).resolve().parents[1]

HOOKS = r'''
static FILE*mainLoopLog;
static unsigned mainLoopNumber,mainBodyNumber,mainBodyOpen,mainBodyWrites[2],mainBodyChanges[2];
static uint64_t mainBodyStart;
static void mainLoopEvent(const char*event,unsigned argument){
 if(!mainLoopLog){mainLoopLog=outputFile("main-loop-events.csv","w");if(!mainLoopLog)abort();
 fputs("event,frame,master_cycles,loop,body,pc,task,manager,scene,argument,s2_writes,s3_writes,s2_changed,s3_changed,body_start\n",mainLoopLog);}
 fprintf(mainLoopLog,"%s,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%" PRIu64 "\n",event,frameNumber,stamp(),mainLoopNumber,mainBodyNumber,sourcePC(),rawLong(0x204)&0xffffff,rawWord(0x8004),rawWord(0x8008),argument,mainBodyWrites[0],mainBodyWrites[1],mainBodyChanges[0],mainBodyChanges[1],mainBodyStart);
}
static void mainLoopObserve(unsigned kind,unsigned at,unsigned width,unsigned value){
 if(renderPath)return;
 unsigned pc=sourcePC(),sp=cpu->dar[15]&0xffffff;
 if(kind==1&&pc==0x92dc&&at==0xf9a1&&width==1){
  if(value!=255||mainBodyOpen||(rawLong(0x204)&0xffffff)!=0xff0060)abort();
  mainLoopNumber++;mainLoopEvent("loop_begin",value);
 }
 if(kind==1&&width==2&&(pc==0x92f2||pc==0x92f6)&&(at&65535)==((sp+2)&65535)){
  if(value!=pc||(rawLong(0x204)&0xffffff)!=0xff0060)abort();
  if(pc==0x92f2){if(mainBodyOpen)abort();mainBodyOpen=1;mainBodyNumber++;mainBodyStart=stamp();memset(mainBodyWrites,0,sizeof mainBodyWrites);memset(mainBodyChanges,0,sizeof mainBodyChanges);mainLoopEvent("body_begin",value);}
  else {if(!mainBodyOpen)abort();mainLoopEvent("body_complete",value);mainBodyOpen=0;}
 }
 if(kind==0&&pc==0x9322&&at==0xf9a1&&width==1)mainLoopEvent(value?"sleep_flag_set":"sleep_bypassed",value);
 if(mainBodyOpen&&kind==3&&at>=0x914000&&at+width<=0x91c000){
  unsigned layer=at>=0x918000?0:1;
  unsigned before=width==2?willowMotionWord(cps+at-0x900000):cps[at-0x900000];
  mainBodyWrites[layer]++;mainBodyChanges[layer]+=before!=(value&(width==2?65535:255));
 }
}
static void mainLoopCheckCode(void){
 static const struct{unsigned at,value;} code[]={
  {0x92d8,0x50ed79a1},{0x92ee,0x610000ce},{0x92f2,0x4eb818bc},
  {0x931e,0x4a2d79a1},{0x9322,0x67b4303c},{0x9324,0x303c0001},
  {0x9328,0x4eb81750},{0x158c,0x422d79a1}};
 for(unsigned n=0;n<sizeof code/sizeof*code;n++)for(unsigned path=0;path<2;path++)
  if(willowMotionLong(programs[path]+code[n].at)!=code[n].value)abort();
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform',type=Path,default=ROOT/'build/native-background-publication/target-live-v7-frozen')
    p.add_argument('--prior',type=Path,default=ROOT/'build/native-background-publication/observer-live-v7')
    p.add_argument('--output',type=Path,default=ROOT/'build/native-main-loop-cadence/v7')
    a = p.parse_args();out=a.output.resolve()
    binary,manifest=background.build(a.platform,ROOT/'build/native-background-residency/prepared-v1',out,collect_register_failures=True)
    generated=out/'test_native_render_cadence.c';text=generated.read_text()
    text=background.cadence.replace(text,'static unsigned sourcePC(void);','static unsigned sourcePC(void);\nstatic void mainLoopObserve(unsigned,unsigned,unsigned,unsigned);')
    needle='static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){'
    text=background.cadence.replace(text,needle,needle+'\n mainLoopObserve(kind,at,width,value);')
    text=background.cadence.replace(text,'int main(',HOOKS+'\nint main(')
    text=background.cadence.replace(text,' if(!backgroundInit(core))return 2;',' if(!backgroundInit(core))return 2;\n mainLoopCheckCode();')
    text=background.cadence.replace(text,' backgroundFinish();',' if(mainLoopLog)fclose(mainLoopLog);\n backgroundFinish();')
    generated.write_text(text);subprocess.run(manifest['command'],cwd=ROOT,check=True)
    manifest.update(generated_sha256=background.sha(generated),binary_sha256=background.sha(binary))
    manifest['inputs'][str(Path(__file__).resolve())]=background.sha(Path(__file__))
    core=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
    with(out/'observer.log').open('w') as log:
        result=subprocess.run([str(binary),str(core),'1800','--coin-route',str(out)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
    manifest['run']={'exit_code':result.returncode,'frames':1800,'core_sha256':background.sha(core),'log_sha256':background.sha(out/'observer.log')}
    (out/'observer-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    assert result.returncode==1,'Expected unchanged explicit register failures'
    names=['register-failures.csv','background-summary.json','render-summary.json','frames.csv','background-publications.csv','render-events.csv','producer-epochs.csv']
    for name in names:assert(out/name).read_bytes()==(a.prior/name).read_bytes(),('Read-only hook changed target observations',name)
    evidence={'scope':__doc__,'unchanged_files':{name:background.sha(out/name) for name in names},'events_sha256':background.sha(out/'main-loop-events.csv'),'source_register_failure_collection_retained':True}
    (out/'read-only-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(json.dumps(evidence,indent=2))


if __name__=='__main__':main()
