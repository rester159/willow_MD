/* Host guard adversarial tests. Includes the exact generated read-only observer;
 * this does not emulate target execution or replace actual core evidence. */
#define main original_observer_main
#include OBSERVER_SOURCE
#undef main
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
static ForestCoreCycles mockCPU;
static uint8_t mockRAM[65536],mockVRAM[65536],mockCRAM[128],mockVSRAM[128],mockRegs[32];
static uint32_t mockFIFO[4],mockDMA;
static uint16_t mockStatus;
static void setword(unsigned at,unsigned v){ram[at^1]=v>>8;ram[(at+1)^1]=v;}
static void setlong(unsigned at,unsigned v){setword(at,v>>16);setword(at+2,v);}
static void cacheword(unsigned at,unsigned v){setword(FAR_BASE+at,v);}
static void cachelong(unsigned at,unsigned v){setlong(FAR_BASE+at,v);}
static void setup(void){
 cpu=&mockCPU;ram=mockRAM;renderVRAM=mockVRAM;renderCRAM=mockCRAM;renderVSRAM=mockVSRAM;renderRegs=mockRegs;
 renderFIFO=mockFIFO;renderDMALength=&mockDMA;renderStatus=&mockStatus;renderPath=0;
 farEvents=tmpfile();farBatchStates=tmpfile();farTransferData=tmpfile();
 for(unsigned n=0;n<165;n++){
  unsigned pattern=farOraclePool[n*2+1];farPaths[0].owner[n]=pattern;cacheword(n*2,pattern);
  memcpy(farPaths[0].expected+n*32,farOraclePatterns+pattern*32,32);
  for(unsigned b=0;b<32;b++)renderVRAM[(farTile(n)*32+b)^1]=farOraclePatterns[pattern*32+b];
 }
 for(unsigned n=0;n<16;n++)ram[(FAR_BASE+FAR_O(SLOTS)+n)^1]=255;
 cacheword(FAR_O(KEY),65535);cacheword(FAR_O(PHASE),1);cachelong(FAR_O(RESIDENCY_EPOCH),1);
 cpu->pc=RENDER_FAR_INIT_BEGIN+2;renderRegs[15]=2;
}
static void transferSetup(void){
 farPhase(11);struct FarObserver*p=farPaths;
 unsigned pattern=0;
 for(unsigned n=1;n<3512&&!pattern;n++)if(farOracleFixed[n]==65535){unsigned found=0;for(unsigned s=0;s<165;s++)found|=p->owner[s]==n;if(!found)pattern=n;}
 if(!pattern)_exit(3);
 p->open=1;p->steps=1;p->slot[0]=0;p->pattern[0]=pattern;p->firstControl=1;
 cacheword(FAR_O(PHASE),3);cacheword(FAR_O(OWNER),65535);cachelong(FAR_O(RESIDENCY_EPOCH),2);
 cpu->pc=RENDER_BG_FAR_UPLOAD_BEGIN+2;
}
static void command(void){unsigned address=farTile(0)*32,first=0x4000|(address&0x3fff),second=address>>14;farVDPBefore(0xc00004,2,first);farVDPAfter(0xc00004,first);farVDPBefore(0xc00004,2,second);farVDPAfter(0xc00004,second);}
static void counterTest(unsigned test){
 unsigned original[21]={0};farLatchBoundaries=tmpfile();latchSelector=0x9000;latchCaptures=1;latchWords=32;latchExpectedWords=1024;
 original[16]=FAR_LATCH_LOOP_BOUNDARY;original[8]=0xff26a0;original[9]=0x900040;original[10]=original[0]=FAR_LATCH_CHUNK_TABLE+2;
 original[1]=FAR_LATCH_CHUNK_TABLE+64;original[2]=0x2660;
 for(unsigned n=0;n<16;n++)cpu->dar[n]=original[n];cpu->dar[9]=0x20c040;cpu->dar[15]=0xfff000;
 setword(0xf00c,256);setword(0xf00e,0);setlong(0xf010,32);
 for(unsigned n=0;n<32;n++)latchSeenWords[n]=1;
 for(unsigned p=0;p<2;p++){unsigned at=FAR_LATCH_LOOP_BOUNDARY-0x100000;platforms[p][at]=0xb5;platforms[p][at+1]=0xc1;platforms[p][at+2]=0x66;platforms[p][at+3]=0xcc;}
 unsigned nativePC=original[16],observed=0;
 switch(test){
 case 26:farLatchPending=1;observed=1;break;
 case 27:farLatchPending=1;frameNumber++;break;
 case 28:original[16]+=2;break;
 case 29:nativePC+=2;break;
 case 30:platforms[0][FAR_LATCH_LOOP_BOUNDARY-0x100000]^=1;break;
 case 31:platforms[1][FAR_LATCH_LOOP_BOUNDARY-0x100000]^=1;break;
 case 32:latchCaptures=2;break;
 case 33:cpu->dar[8]+=2;break;
 case 34:original[8]+=2;break;
 case 35:cpu->dar[9]+=2;break;
 case 36:original[9]+=2;break;
 case 37:cpu->dar[10]+=2;break;
 case 38:cpu->dar[1]+=2;break;
 case 39:cpu->dar[2]+=2;break;
 case 40:setword(0xf00c,255);break;
 case 41:setlong(0xf010,31);break;
 case 42:latchSeenWords[31]=0;break;
 case 43:latchWords=31;break;
 case 44:latchSelector=0;break;
 }
 unsigned okay=farLatchCounterCurrent(observed,original,nativePC);
 if(okay!=(test==25||test==26))_exit(5);
 if(test==26&&farLatchPending)_exit(6);
}
static void viewTest(unsigned test){
 unsigned old[4]={65534,768,65503,1792},now[4]={0,768,65504,1792};
 static const unsigned offsets[]={0,6,8,14,20,30,36,44,52,60,68};
 forestViewLog=tmpfile();
 for(unsigned n=0;n<4;n++)setword(0xf446+n*2,old[n]);
 for(unsigned n=0;n<4;n++){cpu->pc=forestViewLoadNextPC[n];forestBGRead(0xf446+n*2,2,old[n]);}
 unsigned saved=FOREST_VIEW_ACQUISITION_END+(test<=55?offsets[test-45]:0);
 if(test==56)saved++;
 if(test==57)saved+=2;
 if(test==58)saved-=2;
 if(test==59)saved=FOREST_VIEW_MARKER_NEXT;
 if(test==61)forestViewAcquisitions[0].count=3;
 if(test==68)setword(0xf446,123);
 setlong(0xf004,saved);cpu->pc=CADENCE_IRQ+2;forestBGBeforeWrite(0xf000,2,0x2600);
 if(test==60)forestBGBeforeWrite(0xf000,2,0x2600);
 unsigned order[4]={0,2,1,3},pcs[4]={0x19a2,0x1a20,0x19b4,0x1a34};
 if(test==65){order[0]=2;order[1]=0;}
 for(unsigned at=0;at<4&&test!=62;at++){
  unsigned n=order[at];cpu->pc=pcs[n];cpu->dar[13]=test==67?0xff8002:0xff8000;cpu->dar[0]=now[n]+(test==66);
  forestBGBeforeWrite(0xf446+n*2,2,now[n]);setword(0xf446+n*2,now[n]);
 }
 for(unsigned n=0;n<4;n++)willowMotionPutWord(ports+0x110+n*2,old[n]);
 if(test==63)willowMotionPutWord(ports+0x110,123);
 if(test==64)old[0]=123;
 cpu->pc=FOREST_VIEW_MARKER_NEXT;forestBGPrepareView(old,1);
}
static void exercise(unsigned test){
 setup();
 if(test>=45){viewTest(test);return;}
 if(test>=25){counterTest(test);return;}
 switch(test){
 case 1:cachelong(FAR_O(RESIDENCY_EPOCH),0);break;
 case 2:cacheword(FAR_O(OWNER),0);break;
 case 3:renderVRAM[(farTile(0)*32)^1]^=1;break;
 case 4:renderVRAM[0xe000^1]=1;break;
 case 5:setword(RENDER_BG_FAR_PREPARED_WORDS&65535,759);break;
 case 6:cpu->pc=RENDER_FAR_INIT_END;break;
 case 7:renderPaths[0].ownershipArmed=1;break;
 case 8:farPaths[0].initialized=1;break;
 case 9:cacheword(FAR_O(MISSING),1);break;
 }
 if(test<=9){farPhase(11);return;}
 transferSetup();
 if(test==10){farPaths[0].open=0;}
 if(test==11){cacheword(FAR_O(PHASE),1);}
 if(test==12){cacheword(FAR_O(OWNER),farPaths[0].owner[0]);}
 if(test==13){cachelong(FAR_O(RESIDENCY_EPOCH),1);}
 if(test==14){cachelong(FAR_O(PREPARED_RESIDENCY_EPOCH),1);}
 if(test==15){cpu->pc=RENDER_BG_FAR_UPLOAD_END+2;}
 if(test==16){farVDPBefore(0xc00004,1,0x4000);return;}
 if(test==17){farVDPBefore(0xc00004,2,0x8f02);return;}
 if(test==18){farVDPBefore(0xc00000,2,0);return;}
 if(test==19){farVDPBefore(0xc00004,2,0x4000);farVDPAfter(0xc00004,0x4000);farVDPBefore(0xc00004,2,0);return;}
 command();
 for(unsigned n=0;n<16;n++){
  unsigned value=willowMotionWord(farOraclePatterns+farPaths[0].pattern[0]*32+n*2);
  if(test==20&&!n)value^=1;
  farVDPBefore(0xc00000,2,value);
  unsigned at=farPaths[0].lastAddress;renderVRAM[at^1]=value>>8;renderVRAM[(at+1)^1]=value;
  if(test==21&&!n)renderVRAM[at^1]^=1;
  if(test==22&&!n)mockDMA=1;
  farVDPAfter(0xc00000,value);
 }
 if(test==23){farVDPBefore(0xc00000,2,0);return;}
 if(farPaths[0].words!=16||farPaths[0].done!=1||farPaths[0].epoch!=2)_exit(4);
}
int main(void){
 unsigned cases=0;
 for(unsigned n=0;n<69;n++){
  pid_t child=fork();if(child<0)return 2;
  if(!child){exercise(n);_exit(0);}
  int status;if(waitpid(child,&status,0)!=child)return 2;
  unsigned good=n==0||n==24||(n>=25&&n<=55);
  if(good?(!WIFEXITED(status)||WEXITSTATUS(status)):(!WIFSIGNALED(status)||WTERMSIG(status)!=SIGABRT)){
   fprintf(stderr,"Guard case%u unexpected status%x\n",n,status);return 1;
  }
  cases++;
 }
 printf("Far observer guard tests passed%u:2 transfer positives,23 transfer refusals,20 partial-counter cases,24 view-boundary cases\n",cases);return 0;
}
