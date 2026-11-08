/* Adversarial test of the exact generated observer's pointer/join IRQ gate.
 * Mock hardware-frame stores test this predicate only; the continuous core run
 * independently proves the following actual IRQ and saved-frame readback. */
#define main original_observer_main
#include OBSERVER_SOURCE
#undef main
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
static ForestCoreCycles mockCPU;
static uint8_t mockRAM[65536];
static unsigned mockSR=0x2610;
static unsigned getRegister(int n){return n==17?mockSR:0;}
static void word(unsigned at,unsigned value){ram[at^1]=value>>8;ram[(at+1)^1]=value;}
static void longword(unsigned at,unsigned value){word(at,value>>16);word(at+2,value);}
static void fail(void){_exit(7);}
static void exercise(unsigned test){
 cpu=&mockCPU;ram=mockRAM;renderGetRegister=getRegister;renderPath=0;
 const RenderPointerStoreSite*a=0,*b=0;
 for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++){
  const RenderPointerStoreSite*s=renderPointerStoreSites+n;
  if(s->kind!=1||s->size!=8||s->reg!=16||s->publishing==1)continue;
  if(!a)a=s;else if(s->displacement==a->displacement){b=s;break;}
 }
 if(!a||!b)fail();
 unsigned pc=a->pc+8,prior=test==0?a->pc:b->pc,base=(0x7400+a->displacement)&65535;
 for(unsigned path=0;path<2;path++){
  uint8_t*p=platforms[path]+(a->pc-0x100000);
  willowMotionPutWord(p,0x2d7c);willowMotionPutLong(p+2,path?0x20d000:0x908000);willowMotionPutWord(p+6,a->displacement);
  p=platforms[path]+(b->pc-0x100000);
  willowMotionPutWord(p,0x2d7c);willowMotionPutLong(p+2,path?0x20c000:0x900000);willowMotionPutWord(p+6,b->displacement);
  willowMotionPutWord(p+8,0x6000);willowMotionPutWord(p+10,pc-(b->pc+10));
 }
 unsigned original=test==0?0x908000:0x900000;
 longword(base,original);
 for(unsigned k=0;k<4;k++){owner[base+k]=prior;savedValue[base+k]=original;savedByteIndex[base+k]=k;savedRegister[base+k]=16;}
 cpu->dar[14]=0xff7400;cpu->dar[15]=0xffffffee;cpu->pc=pc;
 uint8_t*ca=platforms[0]+(a->pc-0x100000),*cb=platforms[1]+(a->pc-0x100000);
 uint8_t*pa=platforms[0]+(b->pc-0x100000),*pb=platforms[1]+(b->pc-0x100000);
 unsigned candidate=a->pc,firstAt=0xffee,firstValue=pc>>16;
 if(test>=27){
  candidate=FAR_IRQ_AUX_MOVEM_PC;pc=candidate+4;cpu->pc=pc;firstValue=pc>>16;
  willowMotionPutLong(rom+candidate,FAR_IRQ_AUX_MOVEM_WORD);
 }
 switch(test){
 case 2:ca[0]^=1;break;case 3:cb[0]^=1;break;
 case 4:ca[7]^=2;break;case 5:cb[5]^=1;break;
 case 6:for(unsigned k=0;k<4;k++)owner[base+k]=prior+2;break;
 case 7:pa[0]^=1;break;case 8:pa[8]^=1;break;
 case 9:pb[11]^=2;break;case 10:pa[11]^=2;pb[11]^=2;break;
 case 11:ram[base^1]^=1;break;case 12:owner[base+3]^=2;break;
 case 13:savedValue[base+2]^=1;break;case 14:savedByteIndex[base+1]=0;break;
 case 15:savedRegister[base+1]=9;break;case 16:cpu->dar[14]=0xff8000;break;
 case 17:cpu->pc+=2;break;case 18:cpu->dar[15]-=2;break;
 case 19:mockSR=0x2010;break;case 20:firstValue^=1;break;
 case 25:pa[7]^=2;pb[7]^=2;break;
 case 26:for(unsigned k=0;k<4;k++)owner[base+k]=0;break;
 case 28:cpu->pc+=2;break;case 29:rom[candidate+3]^=1;break;
 case 30:candidate+=2;break;case 31:cpu->dar[15]=0xff7400;break;
 }
 unsigned accepted=renderExceptionStore(candidate,firstAt,firstValue);
 if(test>=2&&test<=20){if(accepted)fail();return;}
 if(test==25||test==26||test>=28){if(accepted)fail();return;}
 if(!accepted||renderPaths[0].exceptionStage!=1)fail();
 if(test==23)cpu->dar[15]-=2;
 renderExceptionStore(candidate,0xfff0,(pc&65535)^(test==21));
 cpu->dar[15]=0xffffffec;
 renderExceptionStore(candidate,test==24?0xffea:0xffec,test==22?0x4010:0x10);
 if(renderPaths[0].exceptionStage!=3||renderPaths[0].exceptionPC!=pc||renderPaths[0].exceptionSR!=0x10)fail();
}
#ifdef FAR_IRQ_RESERVED_MOVEM_PC
static void sharedBoundary(unsigned kind,unsigned mutation){
 cpu=&mockCPU;ram=mockRAM;renderGetRegister=getRegister;renderPath=0;
 unsigned candidate=kind?FAR_IRQ_LOOKUP_MOVEM_PC:FAR_IRQ_RESERVED_MOVEM_PC;
 unsigned opcode=kind?FAR_IRQ_LOOKUP_MOVEM_WORD:FAR_IRQ_RESERVED_MOVEM_WORD;
#ifdef FAR_IRQ_FINISH_MOVEM_PC
 if(kind==2){candidate=FAR_IRQ_FINISH_MOVEM_PC;opcode=FAR_IRQ_FINISH_MOVEM_WORD;}
#endif
 unsigned pc=candidate+4;
 willowMotionPutLong(rom+candidate,opcode);
 if(!kind)willowMotionPutLong(rom+candidate-4,FAR_IRQ_RESERVED_MOVEM_PREFIX);
#ifdef FAR_IRQ_FINISH_MOVEM_PC
 if(kind==2)willowMotionPutLong(rom+candidate-4,FAR_IRQ_FINISH_MOVEM_PREFIX);
#endif
 cpu->dar[15]=0xffffffee;cpu->pc=pc;
 switch(mutation){
 case 1:rom[candidate+3]^=1;break;
 case 2:cpu->pc+=2;break;
 case 3:candidate+=2;break;
 case 4:cpu->dar[15]-=2;break;
 case 5:mockSR=0x2010;break;
 case 6:if(kind!=1)rom[candidate-1]^=1;else rom[candidate]^=1;break;
 }
 unsigned accepted=renderExceptionStore(candidate,0xffee,(pc>>16)^(mutation==7));
 if(mutation){if(accepted)fail();return;}
 if(!accepted)fail();
 renderExceptionStore(candidate,0xfff0,pc&65535);cpu->dar[15]=0xffffffec;
 renderExceptionStore(candidate,0xffec,0x10);
 if(renderPaths[0].exceptionStage!=3||renderPaths[0].exceptionPC!=pc)fail();
}
#endif
static void copyBoundary(unsigned mutation){
 cpu=&mockCPU;ram=mockRAM;renderGetRegister=getRegister;renderPath=0;
 const RenderPointerStoreSite*s=0;
 for(unsigned n=0;n<RENDER_POINTER_STORE_COUNT;n++)if(renderPointerStoreSites[n].kind==2&&renderPointerStoreSites[n].publishing==0){s=renderPointerStoreSites+n;break;}
 if(!s)fail();
 unsigned candidate=s->pc,pc=candidate+6,src=(0x7400+s->sourceDisplacement)&65535,dst=(0x7400+s->displacement)&65535,value=0x908000;
 for(unsigned path=0;path<2;path++){
  uint8_t*p=platforms[path]+candidate-0x100000;
  willowMotionPutWord(p,0x2d6e);willowMotionPutWord(p+2,s->sourceDisplacement);willowMotionPutWord(p+4,s->displacement);
 }
 longword(src,value);longword(dst,value);
 for(unsigned k=0;k<4;k++){
  owner[src+k]=0x100100;savedValue[src+k]=value;savedByteIndex[src+k]=k;
  owner[dst+k]=candidate;savedValue[dst+k]=value;savedByteIndex[dst+k]=k;savedRegister[dst+k]=17;
 }
 cpu->dar[14]=0xff7400;cpu->dar[15]=0xffffffee;cpu->pc=pc;
 uint8_t*a=platforms[0]+candidate-0x100000,*b=platforms[1]+candidate-0x100000;
 switch(mutation){
 case 1:a[0]^=1;break;case 2:b[1]^=1;break;
 case 3:a[3]^=2;b[3]^=2;break;case 4:a[5]^=2;b[5]^=2;break;
 case 5:ram[src^1]^=1;break;case 6:ram[dst^1]^=1;break;
 case 7:owner[src+3]=0;break;case 8:owner[dst+3]^=2;break;
 case 9:savedValue[src+2]^=1;break;case 10:savedValue[dst+2]^=1;break;
 case 11:savedByteIndex[src+1]=0;break;case 12:savedByteIndex[dst+1]=0;break;
 case 13:savedRegister[dst+1]=16;break;case 14:cpu->dar[14]=0x7400;break;
 case 15:cpu->dar[14]=0xff8000;break;case 16:cpu->dar[14]++;break;
 case 17:cpu->pc+=2;break;case 18:cpu->dar[15]-=2;break;case 19:mockSR=0x2010;break;
 case 20:for(unsigned k=0;k<4;k++)owner[src+k]=0;break;
 }
 unsigned accepted=renderExceptionStore(candidate,0xffee,(pc>>16)^(mutation==21));
 if(mutation){if(accepted)fail();return;}
 if(!accepted)fail();
 renderExceptionStore(candidate,0xfff0,pc&65535);cpu->dar[15]=0xffffffec;renderExceptionStore(candidate,0xffec,0x10);
 if(renderPaths[0].exceptionStage!=3||renderPaths[0].exceptionPC!=pc)fail();
}
int main(void){
 for(unsigned mutation=0;mutation<22;mutation++){
  pid_t child=fork();if(child<0)return 2;
  if(!child){copyBoundary(mutation);_exit(0);}
  int status;if(waitpid(child,&status,0)!=child)return 2;
  if(!WIFEXITED(status)||WEXITSTATUS(status)){fprintf(stderr,"Copy IRQ mutation%u status%x\n",mutation,status);return 1;}
 }
 puts("Passed22 copy IRQ cases:1 positive,21 refusals");fflush(stdout);
 for(unsigned n=0;n<32;n++){
  pid_t child=fork();if(child<0)return 2;
  if(!child){exercise(n);_exit(0);}
  int status;if(waitpid(child,&status,0)!=child)return 2;
  unsigned abortExpected=n>=21&&n<=24;
  if(abortExpected?(!WIFSIGNALED(status)||WTERMSIG(status)!=SIGABRT):(!WIFEXITED(status)||WEXITSTATUS(status))){
   fprintf(stderr,"Pointer IRQ case%u status%x\n",n,status);return 1;
  }
 }
#ifdef FAR_IRQ_RESERVED_MOVEM_PC
 unsigned sharedKinds=2;
#ifdef FAR_IRQ_FINISH_MOVEM_PC
 sharedKinds=3;
#endif
 for(unsigned kind=0;kind<sharedKinds;kind++)for(unsigned mutation=0;mutation<8;mutation++){
  pid_t child=fork();if(child<0)return 2;
  if(!child){sharedBoundary(kind,mutation);_exit(0);}
  int status;if(waitpid(child,&status,0)!=child)return 2;
  if(!WIFEXITED(status)||WEXITSTATUS(status)){fprintf(stderr,"Shared IRQ kind%u mutation%u status%x\n",kind,mutation,status);return 1;}
 }
 printf("Passed%u additional shared IRQ cases:%u positives,%u refusals\n",sharedKinds*8,sharedKinds,sharedKinds*7);
#endif
 puts("Passed32 pointer/auxiliary IRQ cases:3 positives,25 predicate refusals,4 malformed-frame aborts");return 0;
}
