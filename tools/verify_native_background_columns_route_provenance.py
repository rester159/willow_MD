#!/usr/bin/env python3
"""Adversarial checks of the exact observer guards emitted for the columns run."""
from pathlib import Path
import argparse,hashlib,json,subprocess

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def body(text,start):
    begin=text.index(start);at=text.index('{',begin);depth=1;end=at+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[begin:end]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    run=a.run.resolve();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
    sprite=run/'sprite-observer-with-background-hooks.h';bg=run/'frozen-observer-tools/test_native_background_publication.h';generated=run/'test_native_render_cadence.c'
    s=sprite.read_text();b=bg.read_text();g=generated.read_text()
    table=g[g.index('typedef struct {unsigned pc,size,kind,reg;'):g.index('#define RENDER_POINTER_STORE_COUNT')]
    table+='#define RENDER_POINTER_STORE_COUNT (sizeof renderPointerStoreSites/sizeof *renderPointerStoreSites)\n'
    functions='\n'.join([body(b,'static unsigned backgroundInterruptedMovem('),body(s,'static unsigned renderPointerStoreDetails('),body(s,'static unsigned renderExceptionStore(')])
    finish=body(s,'static void renderBeforeWork(');finish=body(finish,'  if(p->exceptionStage){')
    prefix=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>
static uint8_t platforms[2][65536],rom[0x200000],ram[65536];
static unsigned owner[65536],savedValue[65536],savedByteIndex[65536];
static struct {unsigned dar[16];} cpuValue,*cpu=&cpuValue;
typedef struct {unsigned exceptionStage,exceptionPC,exceptionBase,exceptionSR,exceptionFrames;} RenderObserverPath;
static RenderObserverPath renderPaths[2];static unsigned renderPath,pcValue,srValue=0x2600,checks;
static unsigned sourcePC(void){return pcValue;}
static unsigned getRegister(unsigned n){assert(n==17);return srValue;}
static unsigned (*renderGetRegister)(unsigned)=getRegister;
static unsigned renderAllowedInterruptPC(unsigned pc){return pc>=0x102000&&pc<0x10816c;}
static unsigned willowMotionWord(const uint8_t*p){return (unsigned)p[0]<<8|p[1];}
static unsigned willowMotionLong(const uint8_t*p){return (unsigned)p[0]<<24|(unsigned)p[1]<<16|(unsigned)p[2]<<8|p[3];}
static unsigned rawWord(unsigned at){return willowMotionWord(ram+(at&65535));}
static unsigned rawLong(unsigned at){return willowMotionLong(ram+(at&65535));}
static void word(uint8_t*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void longword(uint8_t*p,unsigned v){word(p,v>>16);word(p+2,v);}
static jmp_buf failJump;
static void guardAbort(void){longjmp(failJump,1);}
#define abort() guardAbort()
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Guard failure line%d check%u: %s\n",__LINE__,checks,#x);exit(1);}checks++;}while(0)
'''
    tests=r'''
static void reset(void){memset(renderPaths,0,sizeof renderPaths);memset(ram,0,sizeof ram);pcValue=0x1027ec;cpu->dar[15]=0xffffee;srValue=0x2600;word(platforms[0]+0x27ea,0x2f03);word(platforms[1]+0x27ea,0x2f03);renderGetRegister=getRegister;}
static void first(void){CHECK(renderExceptionStore(0x1027ea,0xffee,0x10));CHECK(renderPaths[0].exceptionStage==1);}
static void low(void){CHECK(renderExceptionStore(0x1027ea,0xfff0,0x27ec));}
static void sr(void){cpu->dar[15]=0xffffec;CHECK(renderExceptionStore(0x1027ea,0xffec,4));}
int main(int argc,char**argv){
 CHECK(argc==2);FILE*f=fopen(argv[1],"rb");CHECK(f!=0);CHECK(fread(rom+0x100010,1,65520,f)>0);fclose(f);
 unsigned reg,index,value;
 for(unsigned site=0;site<2;site++){
  const RenderPointerStoreSite*s=renderPointerStoreSites+site;cpu->dar[14]=0xff7550;
  CHECK(s->size==8&&s->kind==1&&s->displacement==-134);
  CHECK(renderPointerStoreDetails(s->pc,0x74ca,&reg,&index,&value)&&index==0&&value==willowMotionLong(rom+s->pc+2));
  CHECK(renderPointerStoreDetails(s->pc,0x74cc,&reg,&index,&value)&&index==2);
  CHECK(!renderPointerStoreDetails(s->pc,0x74c8,&reg,&index,&value));
  cpu->dar[14]=0xff7000;CHECK(!renderPointerStoreDetails(s->pc,0x6f7a,&reg,&index,&value));
  cpu->dar[14]=0xff7800;CHECK(!renderPointerStoreDetails(s->pc,0x777a,&reg,&index,&value));
 }
 CHECK(!renderPointerStoreDetails(0x104680,0x74ca,&reg,&index,&value));
 reset();first();low();sr();word(ram+0xffec,4);longword(ram+0xffee,0x1027ec);finish(0xffea,0x2600);CHECK(renderPaths[0].exceptionFrames==1&&!renderPaths[0].exceptionStage);
 for(unsigned variant=0;variant<12;variant++){
  reset();unsigned at=0xffee,value=0x10,ownerPC=0x1027ea;
  switch(variant){
  case 0:word(platforms[0]+0x27ea,0x2f02);break;
  case 1:word(platforms[1]+0x27ea,0x4e71);break;
  case 2:ownerPC=0x1027e8;break;
  case 3:pcValue++;break;
  case 4:cpu->dar[15]=0xff7400;at=0x7400;break;
  case 5:cpu->dar[15]=0xffffff;at=0xffff;break;
  case 6:at+=2;break;
  case 7:value++;break;
  case 8:srValue=0x2000;break;
  case 9:renderGetRegister=0;break;
  case 10:ownerPC=0x1027ee;pcValue=0x1027f0;break;
  case 11:ownerPC=0x104682;pcValue=0x10468a;break;
  }
  CHECK(!renderExceptionStore(ownerPC,at,value)&&!renderPaths[0].exceptionStage);
 }
 for(volatile unsigned variant=0;variant<6;variant++){
  reset();first();
  if(!setjmp(failJump)){
   if(variant==0)renderExceptionStore(0x1027ea,0xfff0,0x27ee);
   else if(variant==1)renderExceptionStore(0x1027ea,0xffee,0x27ec);
   else{
    low();cpu->dar[15]=0xffffec;
    if(variant==2)renderExceptionStore(0x1027ea,0xffec,0x8004);
    else{sr();word(ram+0xffec,4);longword(ram+0xffee,0x1027ec);
     if(variant==3)word(ram+0xffec,5);
     if(variant==4)longword(ram+0xffee,0x1027ee);
     finish(0xffea,variant==5?0x2000:0x2600);
    }
   }
   CHECK(0);
  }else CHECK(renderPaths[0].exceptionFrames==0);
 }
 printf("PASS %u exact emitted guard checks;12 first-word refusals and6 sequence/frame corruptions rejected.\n",checks);return 0;
}
'''
    code=prefix+table+functions+'\nstatic void finish(unsigned at,unsigned value){RenderObserverPath*p=renderPaths+renderPath;'+finish+'}\n'+tests
    source=out/'guard.c';source.write_text(code);binary=out/'guard'
    cmd=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(source),'-o',str(binary)]
    subprocess.run(cmd,check=True)
    # Original immutable platform supplies actual source immediates to the guard.
    manifest=json.loads((run/'observer-manifest.json').read_text());platform=next(Path(n)for n in manifest['inputs']if n.endswith('/platform-original.bin'))
    result=subprocess.run([str(binary),str(platform)],capture_output=True,text=True);(out/'test.log').write_text(result.stdout+result.stderr);assert result.returncode==0,result.stderr
    evidence={'status':'pass','scope':__doc__,'inputs':{str(p):sha(p)for p in [sprite,bg,generated,platform,Path(__file__).resolve()]},'outputs':{p.name:sha(p)for p in [source,binary,out/'test.log']},'result':result.stdout.strip(),'command':cmd}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n');print(result.stdout.strip())
if __name__=='__main__':main()
