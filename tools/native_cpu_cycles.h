#ifndef WILLOW_NATIVE_CPU_CYCLES_H
#define WILLOW_NATIVE_CPU_CYCLES_H
#include <stddef.h>
#include <limits.h>
/* Optional host-only Genesis Plus GX diagnostic. Public core prefix layout:
 * https://github.com/ekeeke/Genesis-Plus-GX/blob/5a8389c/core/m68k/m68k.h
 * m68k_run takes an absolute master-clock target, not a duration. Its exposed
 * m68k_cycles() reports the current opcode's base timing, NOT elapsed cycles.
 * Read this ABI only after checking all registers against m68k_get_reg, then
 * calibrate instruction stepping with NOPs. The only internal write restores
 * the validated refresh-clock field omitted by the core's save-state format.
 * CPU-only execution excludes concurrent VDP/Z80/IRQ work; this is not FPS. */
typedef struct {
 uint8_t*base;
 unsigned(*readByte)(unsigned);unsigned(*readWord)(unsigned);
 void(*writeByte)(unsigned,unsigned);void(*writeWord)(unsigned,unsigned);
} NativeCycleMapABI;
typedef struct {
 NativeCycleMapABI map[256];
 uint32_t idle[3];int32_t clock,refresh;uint32_t target;
 uint32_t registers[16],pc;
} NativeCyclePrefixABI;
static struct {
 void*cpu;void(*run)(unsigned);size_t(*stateSize)(void);
 bool(*save)(void*,size_t);bool(*restore)(const void*,size_t);
 void*state;size_t bytes;
 uint8_t beforeRAM[65536],beforeSRAM[65536],afterRAM[65536],afterSRAM[65536];
 uint64_t*cost;uint32_t*hits;
 uint64_t total,peak,instructions;unsigned updates;
 FILE*csv,*romCSV,*fixedCSV;unsigned romWordPC,fixedBytePC;char prefix[512];int calibrated;
} nativeCycles;
static NativeCyclePrefixABI nativeCycleContext(void)
{NativeCyclePrefixABI c;memcpy(&c,nativeCycles.cpu,sizeof c);return c;}
static int32_t nativeCycleClock(void)
{int32_t c;memcpy(&c,(const uint8_t*)nativeCycles.cpu+offsetof(NativeCyclePrefixABI,clock),sizeof c);return c;}
static int nativeCycleABI(unsigned(*getReg)(int))
{
 NativeCyclePrefixABI c=nativeCycleContext();
 for(unsigned n=0;n<16;n++)if(c.registers[n]!=getReg(n))return 0;
 return c.pc==getReg(16)&&c.clock>=0&&c.target==0;
}
static int nativeCycleInit(void*core,unsigned(*getReg)(int))
{
 if(!getenv("WILLOW_PROFILE_DEMO_CYCLES"))return 0;
 memset(&nativeCycles,0,sizeof nativeCycles);
 nativeCycles.cpu=dlsym(core,"m68k");nativeCycles.run=dlsym(core,"m68k_run");
 nativeCycles.stateSize=dlsym(core,"retro_serialize_size");
 nativeCycles.save=dlsym(core,"retro_serialize");nativeCycles.restore=dlsym(core,"retro_unserialize");
 if(sizeof(unsigned)!=4||!nativeCycles.cpu||!nativeCycles.run||!nativeCycles.stateSize||
    !nativeCycles.save||!nativeCycles.restore||!nativeCycleABI(getReg)){
  fprintf(stderr,"CPU cycle profile: unavailable or incompatible core ABI\n");return -1;
 }
 void(*info)(struct retro_system_info*)=dlsym(core,"retro_get_system_info");
 if(info){struct retro_system_info s={0};info(&s);printf("CPU cycle core: %s %s\n",s.library_name?s.library_name:"unknown",s.library_version?s.library_version:"unknown");}
 nativeCycles.bytes=nativeCycles.stateSize();
 nativeCycles.state=malloc(nativeCycles.bytes);
 nativeCycles.cost=calloc(0x100000,sizeof *nativeCycles.cost);
 nativeCycles.hits=calloc(0x100000,sizeof *nativeCycles.hits);
 const char*prefix=getenv("WILLOW_CYCLE_PROFILE_PREFIX");if(!prefix)prefix="build/demo-sequence";
 if(!nativeCycles.bytes||!nativeCycles.state||!nativeCycles.cost||!nativeCycles.hits||strlen(prefix)>450)return -1;
 strcpy(nativeCycles.prefix,prefix);char path[560];snprintf(path,sizeof path,"%s-cycles.csv",prefix);
 nativeCycles.csv=fopen(path,"w");if(!nativeCycles.csv)return -1;
 fputs("update,master_cycles,cpu_cycles,instructions\n",nativeCycles.csv);
 if(getenv("WILLOW_PROFILE_ROM_READS")){
  snprintf(path,sizeof path,"%s-rom-reads.csv",prefix);nativeCycles.romCSV=fopen(path,"w");
  if(!nativeCycles.romCSV)return -1;fputs("update,address\n",nativeCycles.romCSV);
 }
 if(getenv("WILLOW_PROFILE_FIXED_READS")){
  snprintf(path,sizeof path,"%s-fixed-reads.csv",prefix);nativeCycles.fixedCSV=fopen(path,"w");
  if(!nativeCycles.fixedCSV)return -1;fputs("update,address\n",nativeCycles.fixedCSV);
 }
 return 0;
}
static void nativeCyclePutWord(uint8_t*ram,unsigned at,unsigned value)
{ram[at^1]=value>>8;ram[(at+1)^1]=value;}
static int nativeCycleRestore(uint8_t*ram,uint8_t*sram,NativeCyclePrefixABI before)
{
 if(!nativeCycles.restore(nativeCycles.state,nativeCycles.bytes))return -1;
 /* Cartridge battery RAM may be omitted by libretro save states. */
 memcpy(ram,nativeCycles.beforeRAM,65536);memcpy(sram,nativeCycles.beforeSRAM,65536);
 NativeCyclePrefixABI c=nativeCycleContext();
 if(c.clock!=before.clock||c.pc!=before.pc||
    memcmp(c.registers,before.registers,sizeof c.registers)){
  fprintf(stderr,"CPU cycle profile: save-state CPU/clock round trip is not exact (%d/%d, refresh %d/%d)\n",c.clock,before.clock,c.refresh,before.refresh);return -1;
 }
 /* state.c saves cycles but not refresh_cycles; system_reset during loading
  * leaves a different refresh phase. Restore this exact pre-call value only.
  * The NOP calibration checks the field's live update rule before profiling. */
 memcpy((uint8_t*)nativeCycles.cpu+offsetof(NativeCyclePrefixABI,refresh),&before.refresh,sizeof before.refresh);
 return 0;
}
static int nativeCycleMeasure(uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),const char*name,unsigned step)
{
 if(!nativeCycles.csv)return 0;
 if(!nativeCycleABI(getReg))return -1;
 NativeCyclePrefixABI before=nativeCycleContext();
 memcpy(nativeCycles.beforeRAM,ram,65536);memcpy(nativeCycles.beforeSRAM,sram,65536);
 if(!nativeCycles.save(nativeCycles.state,nativeCycles.bytes))return -1;
 if(!nativeCycles.calibrated){
  for(unsigned n=0;n<64;n++)nativeCyclePutWord(ram,0xf800+n*2,0x4e71);
  setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
  for(unsigned n=0;n<64;n++){
   NativeCyclePrefixABI previous=nativeCycleContext();int32_t start=previous.clock;
   nativeCycles.run((unsigned)start+1);NativeCyclePrefixABI current=nativeCycleContext();
   int32_t delta=current.clock-start;
   /* Four 68000 cycles at master/7, plus the core's optional two-cycle refresh. */
   if(getReg(16)!=0xfff802+n*2||(delta!=28&&delta!=42)||
      current.refresh!=(delta==42?start+896:previous.refresh)){
    fprintf(stderr,"CPU cycle profile: NOP calibration failed at %u, PC %x delta %d\n",n,getReg(16),delta);
    nativeCycleRestore(ram,sram,before);return -1;
   }
  }
  if(nativeCycleRestore(ram,sram,before))return -1;
  nativeCycles.calibrated=1;
  puts("CPU cycle profile: public-prefix register ABI, 64 NOP/refresh timings and CPU/clock restoration validated; omitted refresh phase explicitly restored");
 }
 unsigned address=fullSymbol(symbols,name);if(!address)return -1;
 if(nativeCycles.romCSV&&!nativeCycles.romWordPC){nativeCycles.romWordPC=fullSymbol(symbols,"sourceDemoWord");if(!nativeCycles.romWordPC)return -1;}
 if(nativeCycles.fixedCSV&&!nativeCycles.fixedBytePC){nativeCycles.fixedBytePC=fullSymbol(symbols,"sourceFixedByte");if(!nativeCycles.fixedBytePC)return -1;}
 nativeCyclePutWord(ram,0xf800,0x4eb9);nativeCyclePutWord(ram,0xf802,address>>16);
 nativeCyclePutWord(ram,0xf804,address);nativeCyclePutWord(ram,0xf806,0x60fe);
 setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
 int32_t start=nativeCycleClock();unsigned instructions=0;int failed=0;
 while(getReg(16)!=0xfff806){
  int32_t clock=nativeCycleClock();unsigned pc=getReg(16);
  if(nativeCycles.romCSV&&pc==nativeCycles.romWordPC){
   unsigned sp=getReg(15),offset=(sp&65535)+4;
   if((sp&0xff0000)!=0xff0000||offset>65532){failed=1;break;}
   unsigned at=0;for(unsigned n=0;n<4;n++)at=(at<<8)|ram[(offset+n)^1];
   fprintf(nativeCycles.romCSV,"%u,%x\n",step+1,at);
  }
  if(nativeCycles.fixedCSV&&pc==nativeCycles.fixedBytePC){
   /* SGDK u16 argument occupies the low half of the four-byte stack slot. */
   unsigned sp=getReg(15),offset=(sp&65535)+6;
   if((sp&0xff0000)!=0xff0000||offset>65534){failed=1;break;}
   unsigned at=((unsigned)ram[offset^1]<<8)|ram[(offset+1)^1];
   fprintf(nativeCycles.fixedCSV,"%u,%x\n",step+1,at);
  }
  if(clock<0||clock>INT_MAX-4096||++instructions>20000000){failed=1;break;}
  nativeCycles.run((unsigned)clock+1);
  int32_t delta=nativeCycleClock()-clock;
  if(delta<=0||delta>1000000){failed=1;break;}
  if(pc<0x200000&&!(pc&1)){nativeCycles.cost[pc/2]+=delta;nativeCycles.hits[pc/2]++;}
 }
 uint64_t elapsed=(uint32_t)(nativeCycleClock()-start);
 if((getReg(0)&65535)!=1)failed=1;
 memcpy(nativeCycles.afterRAM,ram,65536);memcpy(nativeCycles.afterSRAM,sram,65536);
 if(nativeCycleRestore(ram,sram,before))return -1;
 if(failed){fprintf(stderr,"CPU cycle profile: service did not complete (%s)\n",name);return -1;}
 fprintf(nativeCycles.csv,"%u,%llu,%.3f,%u\n",step+1,(unsigned long long)elapsed,elapsed/7.0,instructions);
 fflush(nativeCycles.csv);nativeCycles.total+=elapsed;nativeCycles.instructions+=instructions;
 if(elapsed>nativeCycles.peak)nativeCycles.peak=elapsed;nativeCycles.updates++;return 0;
}
static int nativeCycleCompare(const uint8_t*ram,const uint8_t*sram,unsigned step)
{
 if(!nativeCycles.csv)return 0;
 if(memcmp(ram,nativeCycles.afterRAM,0xf600)||memcmp(sram,nativeCycles.afterSRAM,65536)){
  for(unsigned n=0;n<0xf600;n++)if(ram[n]!=nativeCycles.afterRAM[n]){fprintf(stderr,"CPU cycle state mismatch update %u RAM %04x: %02x/%02x\n",step+1,n^1,ram[n],nativeCycles.afterRAM[n]);break;}
  for(unsigned n=0;n<65536;n++)if(sram[n]!=nativeCycles.afterSRAM[n]){fprintf(stderr,"CPU cycle state mismatch update %u SRAM %04x: %02x/%02x\n",step+1,n,sram[n],nativeCycles.afterSRAM[n]);break;}
  return -1;
 }
 return 0;
}
static int nativeCycleFinish(void)
{
 if(!nativeCycles.csv)return 0;
 fclose(nativeCycles.csv);nativeCycles.csv=NULL;char path[560];
 if(nativeCycles.romCSV){fclose(nativeCycles.romCSV);nativeCycles.romCSV=NULL;}
 if(nativeCycles.fixedCSV){fclose(nativeCycles.fixedCSV);nativeCycles.fixedCSV=NULL;}
 snprintf(path,sizeof path,"%s-cycle-pcs.csv",nativeCycles.prefix);FILE*f=fopen(path,"w");if(!f)return -1;
 fputs("pc,instructions,master_cycles\n",f);
 for(unsigned n=0;n<0x100000;n++)if(nativeCycles.hits[n])fprintf(f,"%x,%u,%llu\n",n*2,nativeCycles.hits[n],(unsigned long long)nativeCycles.cost[n]);
 fclose(f);
 printf("CPU-only demo profile: %u updates, %llu master cycles total, %llu peak, %llu instructions; all non-stack RAM/SRAM matches normal execution; call overhead included, idle return loop excluded; no VDP/Z80/IRQ concurrency or FPS claim\n",nativeCycles.updates,(unsigned long long)nativeCycles.total,(unsigned long long)nativeCycles.peak,(unsigned long long)nativeCycles.instructions);
 free(nativeCycles.state);free(nativeCycles.cost);free(nativeCycles.hits);return 0;
}
#endif
