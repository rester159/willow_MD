/* opmlog: offline research instrument (never shipped). Runs the unmodified
 * arcade Willow Z80 sound program headlessly in the Genesis Plus GX Z80 core
 * (CPS sound map, as build/native-audio-admission/sequencer/test_mixed.c),
 * issues one sound command through the original 0038 IRQ path and logs every
 * YM2151 register write and OKI port write per 249.69 Hz service tick.
 *
 * Tick 0: IRQ with only the timer-B flag set (0132 polls the latch, loads the
 * cue). Tick k>=1: IRQ with timer A and B flags (effects 0B97, music 040C,
 * then timer B: pending OKI start, latch poll). Both arcade timers are
 * programmed to 14,336 YM clocks (A: 10/11=C8/00, B: 12=F2).
 *
 * Loop: the 2 KiB sound RAM (minus the 0780-07FD call-stack scratch) plus the
 * bank latch after each tick is compared with every earlier state; the first
 * repeat S_T == S_L means ticks L+1..T repeat forever.
 * End: effects stop when both effect slots (D500/D540 pointers) are zero.
 *
 * usage: opmlog CORE.dylib SOUNDCPU.bin CMD OUT.log [--param P] [--max TICKS]
 *        [--then CMD@TICK[:PARAM]]
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
typedef union{uint32_t d;uint16_t w[2];uint8_t b[4];}Pair;
typedef struct{Pair pc,sp,af,bc,de,hl,ix,iy,wz,af2,bc2,de2,hl2;uint8_t r,r2,iff1,iff2,halt,im,i,nmi_state,nmi_pending,irq_state,after_ei;uint32_t cycles;const void*daisy;int(*irq_callback)(int);}Z80Regs;

static uint8_t source[65536],memory[65536];
static uint8_t bank,latch,parameter,ymRegister,status;
static unsigned tick;static FILE*out;static unsigned long ymWrites,okiWrites;

static uint8_t readPort(uint16_t at){
 if(at==0xf008)return latch;if(at==0xf00a)return parameter;if(at==0xf001)return status;
 fprintf(stderr,"bad read %x\n",at);abort();
}
static void writePort(uint16_t at,uint8_t v){
 if(at==0xf004){bank=v&1;memcpy(memory+0x8000,source+0x8000+bank*0x4000,0x4000);}
 else if(at==0xf000)ymRegister=v;
 else if(at==0xf001){if(ymRegister==0x14){if(v&16)status&=~1;if(v&32)status&=~2;}
  fprintf(out,"Y %u %02x %02x\n",tick,ymRegister,v);ymWrites++;}
 else if(at==0xf002){fprintf(out,"O %u %02x\n",tick,v);okiWrites++;}
 else if(at==0xf006){fprintf(out,"W %u f006 %02x\n",tick,v);}
 else{fprintf(stderr,"bad write %x\n",at);abort();}
}
static uint8_t rd(unsigned at){at&=65535;if(at>=0xf000&&at<=0xf00a)return readPort((uint16_t)at);return memory[at];}
static void wr(unsigned at,uint8_t v){at&=65535;if(at>=0xf000&&at<=0xf006)writePort((uint16_t)at,v);else if(at>=0xd000&&at<0xd800)memory[at]=v;else{fprintf(stderr,"Bad write %x\n",at);abort();}}
static int irq(int level){(void)level;return 0xff;}
#define LOAD(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing %s\n",#n);return 2;}
#define STATE 2049
static uint64_t hashState(const uint8_t*s){uint64_t h=1469598103934665603ull;for(unsigned n=0;n<STATE;n++){h^=s[n];h*=1099511628211ull;}return h;}

int main(int argc,char**argv){
 if(argc<5){fprintf(stderr,"usage: opmlog CORE SOUNDCPU CMD OUT [--param P] [--max N] [--then CMD@TICK[:P]]\n");return 2;}
 unsigned cmd=strtoul(argv[3],0,0),param=0,maxTicks=65536,thenCmd=256,thenTick=0,thenParam=0;
 for(int a=5;a<argc;a++){
  if(!strcmp(argv[a],"--param")&&a+1<argc)param=strtoul(argv[++a],0,0);
  else if(!strcmp(argv[a],"--max")&&a+1<argc)maxTicks=strtoul(argv[++a],0,0);
  else if(!strcmp(argv[a],"--then")&&a+1<argc){char*e;thenCmd=strtoul(argv[++a],&e,0);if(*e=='@')thenTick=strtoul(e+1,&e,0);if(*e==':')thenParam=strtoul(e+1,&e,0);}
  else{fprintf(stderr,"bad arg %s\n",argv[a]);return 2;}
 }
 FILE*f=fopen(argv[2],"rb");if(!f||fread(source,1,sizeof source,f)!=sizeof source){fprintf(stderr,"cannot read sound ROM\n");return 2;}fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){fprintf(stderr,"%s\n",dlerror());return 2;}
 LOAD(void,z80_init,(const void*,int(*)(int)));LOAD(void,z80_reset,(void));LOAD(void,z80_run,(unsigned));LOAD(void,z80_get_context,(void*));LOAD(void,z80_set_context,(void*));
 uint8_t**fetch=dlsym(core,"z80_readmap");uint8_t(**readmem)(unsigned)=dlsym(core,"z80_readmem");void(**writemem)(unsigned,uint8_t)=dlsym(core,"z80_writemem");Z80Regs*live=dlsym(core,"Z80");if(!fetch||!readmem||!writemem||!live)return 2;
 z80_init(NULL,irq);z80_reset();for(unsigned n=0;n<64;n++)fetch[n]=memory+n*1024;*readmem=rd;*writemem=wr;
 Z80Regs r={0};z80_set_context(&r);memcpy(memory,source,0x8000);z80_run(1);z80_get_context(&r);if(r.pc.d!=1||r.cycles!=60){fprintf(stderr,"core self-check failed\n");return 2;}

 out=fopen(argv[4],"w");if(!out)return 2;
 /* Same initial state as the admission fixtures: latch history FF, IRQ return to CFFF. */
 memset(memory,0,sizeof memory);memcpy(memory,source,0x8000);memcpy(memory+0x8000,source+0x8000,0x4000);bank=0;
 memory[0xd000]=255;
 uint8_t*states=malloc((size_t)(maxTicks+1)*STATE);uint64_t*hashes=malloc((size_t)(maxTicks+1)*8);if(!states||!hashes)return 2;
 Z80Regs carry={0};long loopStart=-1;unsigned endTick=0;const char*reason="limit";unsigned long long instructions=0;
 uint8_t okiVoices=0;
 fprintf(out,"# opmlog v1: arcade Willow Z80 sound program, YM2151 writes per service tick\n");
 fprintf(out,"cmd %02x\nparam %02x\nrate %.6f\n",cmd,param,3579545.0/14336.0);
 if(thenCmd<256)fprintf(out,"then %02x %u %02x\n",thenCmd,thenTick,thenParam);
 for(tick=0;tick<=maxTicks;tick++){
  latch=cmd;parameter=param;if(thenCmd<256&&tick>=thenTick){latch=thenCmd;parameter=thenParam;}
  status=tick?3:2;ymRegister=0;
  memory[0xd7fc]=0xff;memory[0xd7fd]=0xcf;
  r=carry;r.pc.d=0x38;r.sp.d=0xd7fc;r.cycles=0;z80_set_context(&r);unsigned steps=0;
  while(live->pc.d!=0xcfff&&steps++<200000)z80_run(live->cycles+1);
  if(live->pc.d!=0xcfff){fprintf(stderr,"runaway tick %u pc %x\n",tick,live->pc.d);return 1;}
  z80_get_context(&carry);instructions+=steps;
  uint8_t*s=states+(size_t)tick*STATE;memcpy(s,memory+0xd000,2048);memset(s+0x780,0,0x7fe - 0x780);s[2048]=bank;hashes[tick]=hashState(s);
  endTick=tick;
  /* loop search: only after any scheduled second command has been taken */
  if(tick>0&&(thenCmd>255||tick>thenTick+1)){
   unsigned from=thenCmd<256?thenTick+1:1;
   for(unsigned p=from;p<tick;p++)if(hashes[p]==hashes[tick]&&!memcmp(states+(size_t)p*STATE,s,STATE)){loopStart=p+1;reason="loop";break;}
   if(loopStart>=0)break;
  }
  if(cmd>=0x20&&cmd<0x80&&tick>0&&thenCmd>255){
   const uint8_t*m=memory+0xd000;if(!(m[0x500]|m[0x501]|m[0x540]|m[0x541])){reason="ended";break;}
  }
  if(cmd>=0x20&&cmd<0x80&&thenCmd>255&&tick>=maxTicks)break;
 }
 (void)okiVoices;
 fprintf(out,"end %u %s\n",endTick,reason);
 if(loopStart>=0)fprintf(out,"loop %ld %u\n",loopStart,endTick+1);
 fclose(out);
 printf("cmd %02x: %u ticks (%.2f s), end=%s",cmd,endTick+1,(endTick+1)/(3579545.0/14336.0),reason);
 if(loopStart>=0)printf(", loop ticks %ld..%u (%u ticks, %.3f s)",loopStart,endTick,endTick+1-(unsigned)loopStart,(endTick+1-loopStart)/(3579545.0/14336.0));
 printf(", %lu YM writes, %lu OKI writes, %llu Z80 instructions\n",ymWrites,okiWrites,instructions);
 return 0;
}
