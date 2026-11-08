/* Continuous original scheduler under actual headless Genesis NTSC VBlank.
 * Source and candidate use identical Genesis cadence, not the CPS10MHz clock.
 * No host instruction stepping, CPU register writes, or invented frame waits. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../build/native-cadence/platform-addresses.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],programs[2][0x100000],cps[0x22800],ports[512];
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT||cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} ForestCoreCycles;
static ForestCoreCycles*cpu;static uint8_t*ram;
static const struct {unsigned source,target,size;} regions[]={
 {0x910000,0x200000,0x4000},{0x918000,0x204000,0x4000},
 {0x914000,0x208000,0x4000},{0x900000,0x20c000,0x800},
 {0x908000,0x20d000,0x800},{0x91c000,0x20d800,0x1800},
 {0x922000,0x20c800,0x800}};
static uint8_t sourceRAM[65536],pathSRAM[2][65536];
static unsigned owner[65536],savedWaitReturn[65536],accepted[65536],pointerPairs,d0Residues;
static unsigned savedValue[65536],partialPointerBytes;
static uint8_t savedByteIndex[65536],savedRegister[65536];
static unsigned portsRead,portsWritten,videoAccesses,frameNumber;
static const char*outputDirectory="build/native-cadence";
static FILE*outputFile(const char*name,const char*mode){char path[512];if(snprintf(path,sizeof path,"%s/%s",outputDirectory,name)>=(int)sizeof path)return NULL;return fopen(path,mode);}
static unsigned char savedMaps[5][sizeof(((ForestCoreCycles*)0)->map[0])];
static unsigned sourcePC(void){return cpu->pc&0xffffff;}
static void portBounds(unsigned at,unsigned width){if(at<0x800000||at+width>0x800200){fprintf(stderr,"Unrepresented port %x/%u PC%x frame%u\n",at,width,sourcePC(),frameNumber);abort();}}
static unsigned portRead8(unsigned at){portBounds(at,1);portsRead++;return ports[at-0x800000];}
static unsigned portRead16(unsigned at){portBounds(at,2);if(at&1)abort();portsRead++;return willowMotionWord(ports+(at-0x800000));}
static void portWrite8(unsigned at,unsigned v){portBounds(at,1);portsWritten++;ports[at-0x800000]=v;}
static void portWrite16(unsigned at,unsigned v){portBounds(at,2);if(at&1)abort();portsWritten++;willowMotionPutWord(ports+(at-0x800000),v);}
static void videoBounds(unsigned at,unsigned width){for(unsigned n=0;n<7;n++)if(at>=regions[n].source&&at+width<=regions[n].source+regions[n].size){videoAccesses++;return;}fprintf(stderr,"Unrepresented CPS memory%x/%u PC%x frame%u\n",at,width,sourcePC(),frameNumber);abort();}
static unsigned videoRead8(unsigned at){videoBounds(at,1);return cps[at-0x900000];}
static unsigned videoRead16(unsigned at){videoBounds(at,2);if(at&1)abort();return willowMotionWord(cps+(at-0x900000));}
static void videoWrite8(unsigned at,unsigned v){videoBounds(at,1);cps[at-0x900000]=v;}
static void videoWrite16(unsigned at,unsigned v){videoBounds(at,2);if(at&1)abort();willowMotionPutWord(cps+(at-0x900000),v);}
static unsigned storeOwner(void){
 unsigned next=sourcePC();if(next<4||next>sizeof rom)return 0;
 const uint8_t*p=next<0x100000?programs[0]:rom;
 /* Native MOVEM predecrement context save, or the exact palette pointer field.
  * Every other RAM store invalidates ownership for its bytes. */
 if(willowMotionWord(p+next-4)==0x48e7)return next-4;
 if((next==0xc46||next==0x1282)&&willowMotionLong(p+next-4)==0x2d490030)return next-4;
 return 0;
}
static void workWrite8(unsigned at,unsigned v){at&=65535;ram[at^1]=v;owner[at]=0;}
static void workWrite16(unsigned at,unsigned v){
 at&=65535;if(at&1)abort();ram[at^1]=v>>8;ram[(at+1)^1]=v;unsigned pc=storeOwner();owner[at]=owner[at+1]=pc;
 savedValue[at]=savedValue[at+1]=0;
 if(pc){
  const uint8_t*p=pc<0x100000?programs[0]:rom;
  unsigned value=0,index=0,reg=255,found=0;
  if(willowMotionWord(p+pc)==0x48e7){
   unsigned mask=willowMotionWord(p+pc+2),cursor=cpu->dar[15];
   /* 68000 predecrement MOVEM saves A7..D0 in encoded mask order. The
    * installed core retains the original SP throughout its individual writes.
    * Validate each actual source word against the exact saved register. */
   for(unsigned bit=0;bit<16;bit++)if(mask&(1u<<bit)){
    cursor-=4;
    if(at==(cursor&65535)||at==((cursor+2)&65535)){
     reg=15-bit;index=at==(cursor&65535)?0:2;value=cpu->dar[reg];found=1;break;
    }
   }
  }else if(pc==0xc42||pc==0x127e){
   unsigned base=(cpu->dar[14]+48)&65535;
   if(at==base||at==((base+2)&65535)){reg=9;index=at==base?0:2;value=cpu->dar[9];found=1;}
  }
  if(!found||(reg!=15&&((value>>(index?0:16))&65535)!=(v&65535))){fprintf(stderr,"Unverified context-store layout PC%x addr%x word%x reg%u value%x index%u\n",pc,at,v,reg,value,index);abort();}
  if(reg!=15)for(unsigned n=0;n<2;n++){savedValue[at+n]=value;savedByteIndex[at+n]=index+n;savedRegister[at+n]=reg;}
 }
 if(pc==0x1750)for(unsigned k=0;k<2;k++){
  unsigned site=(at-k*2)&65535,ret=0;
  for(unsigned n=0;n<4;n++)ret=ret<<8|ram[((site+60+n)&65535)^1];
  savedWaitReturn[site]=ret;
 }
}
static void selectProgram(unsigned path){
 for(unsigned page=0;page<16;page++){uint8_t*b=cpu->map[page].base;if(!b)abort();for(unsigned n=0;n<65536;n++)b[n^1]=programs[path][page*65536+n];}
 const unsigned pages[]={0x80,0x90,0x91,0x92,0xff};for(unsigned n=0;n<5;n++)memcpy(cpu->map+pages[n],savedMaps[n],sizeof savedMaps[n]);
 if(!path){
  cpu->map[0x80].base=ports;cpu->map[0x80].read8=portRead8;cpu->map[0x80].read16=portRead16;cpu->map[0x80].write8=portWrite8;cpu->map[0x80].write16=portWrite16;
  for(unsigned p=0x90;p<=0x92;p++){cpu->map[p].base=cps+(p-0x90)*65536;cpu->map[p].read8=videoRead8;cpu->map[p].read16=videoRead16;cpu->map[p].write8=videoWrite8;cpu->map[p].write16=videoWrite16;}
  cpu->map[0xff].write8=workWrite8;cpu->map[0xff].write16=workWrite16;
 }
}
static int samePointer(unsigned a,unsigned b){for(unsigned n=0;n<7;n++)if(a>=regions[n].source&&a<=regions[n].source+regions[n].size&&b==regions[n].target+a-regions[n].source)return 1;return 0;}
static unsigned nativeLong(unsigned at){unsigned value=0;for(unsigned n=0;n<4;n++)value=value<<8|ram[((at+n)&65535)^1];return value;}
static int compareMemory(const uint8_t*sram){
 for(unsigned n=0;n<65536;n++){
  if(!(n&1)&&n<=65532){unsigned a=willowMotionLong(sourceRAM+n),b=nativeLong(n);if(a!=b){
   unsigned pc=owner[n];int context=pc&&owner[n+1]==pc&&owner[n+2]==pc&&owner[n+3]==pc;
   if(samePointer(a,b)&&context){accepted[n]=pc;pointerPairs++;n+=3;continue;}
   if(context&&pc==0x1750&&a==0x900001&&b==0x200001){unsigned ret=savedWaitReturn[n];if((ret==0x932c||ret==0x1ea2||ret==0x1ec4||ret==0x810e)&&willowMotionLong(programs[0]+ret-8)==0x303c0001&&willowMotionLong(programs[0]+ret-4)==0x4eb81750){accepted[n]=pc;d0Residues++;n+=3;continue;}}
  }}
  if(sourceRAM[n]!=ram[n^1]){
   /* A later short stack write may replace only half of an old saved pointer.
    * Compare the surviving byte against the exact original register value
    * observed at its last MOVEM/palette store; never reconstruct a pointer
    * from the unrelated low half currently occupying that stack location. */
   unsigned value=savedValue[n],matched=0,shift=(3-savedByteIndex[n])*8;
   if(owner[n]&&((value>>shift)&255)==sourceRAM[n])for(unsigned r=0;r<7;r++)if(value>=regions[r].source&&value<=regions[r].source+regions[r].size){unsigned target=regions[r].target+value-regions[r].source;if(((target>>shift)&255)==ram[n^1]){matched=1;break;}}
   if(matched){accepted[n]=owner[n];partialPointerBytes++;continue;}
   unsigned at=n&~1u;fprintf(stderr,"Cadence RAM mismatch frame%u at%x original%02x native%02x owner%x; long@%x=%x/%x return+60=%x originalSavedReg%u value%x byte%u\n",frameNumber,n,sourceRAM[n],ram[n^1],owner[n],at,willowMotionLong(sourceRAM+at),nativeLong(at),at<=65472?willowMotionLong(sourceRAM+at+60):0,savedRegister[n],savedValue[n],savedByteIndex[n]);return 1;
  }
 }
 for(unsigned r=0;r<7;r++)if(memcmp(cps+(regions[r].source-0x900000),sram+(regions[r].target-0x200000),regions[r].size)){fprintf(stderr,"Cadence video mismatch frame%u region%u\n",frameNumber,r);return 1;}
 if(memcmp(ports,sram+(CADENCE_IO_MIRROR-0x200000),512)){fprintf(stderr,"Cadence ports mismatch frame%u\n",frameNumber);return 1;}
 if(memcmp(pathSRAM[0]+0xf000,sram+0xf000,0x800)){fprintf(stderr,"Cadence native observation mismatch frame%u\n",frameNumber);return 1;}
 return 0;
}
int main(int argc,char**argv){
 if(argc<2||argc>4)return 2;unsigned frames=argc>=3?(unsigned)strtoul(argv[2],NULL,10):32;if(!frames||frames>36000)return 2;
 unsigned route=argc==4;if(route&&strcmp(argv[3],"--coin-route"))return 2;if(route)outputDirectory="build/native-cadence/route-v1";
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(programs[0],1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/native-cadence/program.bin","rb");if(!f||fread(programs[1],1,0x100000,f)!=0x100000)return 2;fclose(f);
 for(unsigned n=0;n<2;n++){
  uint8_t*p=programs[n];if(willowMotionLong(p+0x68)!=0x1554||willowMotionLong(p+0xa0)!=0x16be||willowMotionWord(p+0x15b8)!=0x4e73)return 2;
  willowMotionPutLong(p,0xffff00);willowMotionPutLong(p+4,CADENCE_BOOT);willowMotionPutLong(p+0x78,CADENCE_IRQ);
  memcpy(p+0x100,"SEGA GENESIS    ",16);memcpy(p+0x1f0,"U               ",16);p[0x1b0]='R';p[0x1b1]='A';p[0x1b2]=0xf8;p[0x1b3]=0x20;willowMotionPutLong(p+0x1b4,0x200000);willowMotionPutLong(p+0x1b8,0x20ffff);
 }
 memcpy(rom,programs[0],0x100000);f=fopen("build/native-cadence/platform.bin","rb");if(!f)return 2;size_t platformBytes=fread(rom+0x100010,1,0x10000,f);if(!platformBytes||!feof(f))return 2;fclose(f);
 f=fopen("build/native-cadence/oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(unsigned,m68k_get_reg,(int));FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));FN(size_t,retro_serialize_size,(void));FN(bool,retro_serialize,(void*,size_t));FN(bool,retro_unserialize,(const void*,size_t));FN(void,retro_get_system_av_info,(struct retro_system_av_info*));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-cadence/oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 if(m68k_get_reg(16)!=CADENCE_BOOT||cpu->pc!=CADENCE_BOOT)return 2;for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 uint8_t*pal=dlsym(core,"vdp_pal");uint16_t*lines=dlsym(core,"lines_per_frame");if(!pal||!lines||*pal||*lines!=262)return 2;
 struct retro_system_av_info av;retro_get_system_av_info(&av);printf("Reference clock: GenesisNTSC, 53693175masterHz,262lines,896040mastercycles/frame, libretro %.9fHz; not CPS10MHz cadence\n",av.timing.fps);
 const unsigned pages[]={0x80,0x90,0x91,0x92,0xff};for(unsigned n=0;n<5;n++)memcpy(savedMaps[n],cpu->map+pages[n],sizeof savedMaps[n]);
 size_t stateSize=retro_serialize_size();void*states[2]={malloc(stateSize),malloc(stateSize)};if(!stateSize||!states[0]||!states[1]||!retro_serialize(states[0],stateSize))return 2;memcpy(states[1],states[0],stateSize);
 memset(cps,0,sizeof cps);memset(ports,255,sizeof ports);memset(pathSRAM,0,sizeof pathSRAM);for(unsigned n=0;n<2;n++)memset(pathSRAM[n]+(CADENCE_IO_MIRROR-0x200000),255,512);
 f=outputFile("frames.csv","w");if(!f)return 2;fputs("frame,irq_entries,source_clock,manager,scene,pc,expiring_task_waits,gameplay_waits,manager_waits,nested_waits,active_without_gameplay_wait,irqs_with_ready_tasks,attract_waits,active_irqs,invalid_wait_contexts,credits,demo,raw_system,raw_player1,route_tick\n",f);
 FILE*irqFile=outputFile("irqs.csv","w");if(!irqFile)return 2;fputs("frame,irq,hv,saved_sr,previous_source_clock,task0_state,task1_state,manager,scene,interrupted_pc,expiring_wait_mask,ready_mask,gameplay_wait_mask,nested_wait_mask,manager_wait_mask,attract_wait_mask\n",irqFile);
 unsigned lastRecordedIRQ=0,routeTicks=0,activeFrames=0,startFrame=~0u;
 for(frameNumber=0;frameNumber<frames;frameNumber++){
  unsigned rawSystem=255,rawPlayer=255;
  if(route){
   /* Source18D0 reads raw800018 (coin/start) and800001 (P1); source1BFA
    * debounces coin history and8186 consumes the actual start-button edge.
    * Source input masks: coin1=01,start1=10,right=01,button1=10. */
   if(frameNumber>=120&&frameNumber<124)rawSystem&=~1u;
   unsigned titleUSP=willowMotionLong(sourceRAM+0x48)&65535;
   unsigned titleReturn=titleUSP<=65472?willowMotionLong(sourceRAM+titleUSP+60):0;
   if(startFrame==~0u&&frameNumber>=140&&sourceRAM[0x82a2]&&willowMotionWord(sourceRAM+0x8000)==4&&sourceRAM[0x40]==1&&willowMotionLong(sourceRAM+0x44)==0x1756&&titleReturn==0x9162)startFrame=frameNumber;
   if(startFrame!=~0u&&frameNumber>=startFrame&&frameNumber<startFrame+4)rawSystem&=~0x10u;
   if(frameNumber&&willowMotionWord(sourceRAM+0x8004)==2&&willowMotionWord(sourceRAM+0x8008)==2&&!sourceRAM[0x8293]){
    activeFrames++;if(routeTicks<180)rawPlayer&=~(1u|((routeTicks%8)<2?0x10u:0));routeTicks++;
   }
  }
  unsigned originalRegs[21],originalCycles=0;
  for(unsigned path=0;path<2;path++){
   selectProgram(path);if(!retro_unserialize(states[path],stateSize))return 2;selectProgram(path);memcpy(sram,pathSRAM[path],65536);
   /* Raw active-low input only, outside the complete hardware-frame call. */
   const unsigned inputPorts[]={0,1,0x11,0x18,0x19,0x1a,0x1c,0x1e};
   for(unsigned n=0;n<8;n++){ports[inputPorts[n]]=255;sram[CADENCE_IO_MIRROR-0x200000+inputPorts[n]]=255;}
   if(route){ports[0x18]=rawSystem;ports[1]=rawPlayer;ports[0x1e]=0xfa;sram[CADENCE_IO_MIRROR-0x200000+0x18]=rawSystem;sram[CADENCE_IO_MIRROR-0x200000+1]=rawPlayer;sram[CADENCE_IO_MIRROR-0x200000+0x1e]=0xfa;}
   retro_run();
   if(!retro_serialize(states[path],stateSize))return 2;memcpy(pathSRAM[path],sram,65536);
   if(!path){for(unsigned n=0;n<65536;n++)sourceRAM[n]=ram[n^1];for(unsigned n=0;n<21;n++)originalRegs[n]=m68k_get_reg(n);originalCycles=cpu->cycles;}
   else{
    if(compareMemory(sram))return 1;
    for(unsigned n=0;n<21;n++)if(m68k_get_reg(n)!=originalRegs[n]&&!samePointer(originalRegs[n],m68k_get_reg(n))){fprintf(stderr,"Cadence register mismatch frame%u r%u original%x native%x\n",frameNumber,n,originalRegs[n],m68k_get_reg(n));return 1;}
    if((unsigned)cpu->cycles!=originalCycles){fprintf(stderr,"Cadence residual-cycle mismatch frame%u original%u native%d\n",frameNumber,originalCycles,cpu->cycles);return 1;}
   }
  }
  unsigned irq=willowMotionLong(sram+0xf000),manager=willowMotionWord(sourceRAM+0x8004),scene=willowMotionWord(sourceRAM+0x8008);
  if(willowMotionLong(sram+0xf024)||irq-lastRecordedIRQ>32){fprintf(stderr,"Invalid source wait context or overwritten native IRQ observations\n");return 1;}
  fprintf(f,"%u,%u,%u,%u,%u,%x",frameNumber,irq,sourceRAM[0x8018],manager,scene,m68k_get_reg(16));for(unsigned n=1;n<10;n++)fprintf(f,",%u",willowMotionLong(sram+0xf000+n*4));fprintf(f,",%u,%u,%02x,%02x,%u\n",sourceRAM[0x82a2],sourceRAM[0x8293],rawSystem,rawPlayer,routeTicks);
  for(unsigned n=lastRecordedIRQ+1;n<=irq;n++){
   const uint8_t*p=sram+0xf040+((n-1)&31)*32;if(willowMotionLong(p)!=n)return 1;
   fprintf(irqFile,"%u,%u,%04x,%04x,%u,%u,%u,%u,%u,%x",frameNumber,n,willowMotionWord(p+4),willowMotionWord(p+6),p[8],p[9],p[10],willowMotionWord(p+12),willowMotionWord(p+14),willowMotionLong(p+16));for(unsigned offset=20;offset<32;offset+=2)fprintf(irqFile,",%04x",willowMotionWord(p+offset));fputc('\n',irqFile);
  }
  lastRecordedIRQ=irq;
  if(!route||!(frameNumber%32))printf("Cadence frame%u IRQ%u sourceClock%u state%u/%u PC%x mainWait%u gameplay%u activeBusy%u credits%u demo%u routeTicks%u\n",frameNumber,irq,sourceRAM[0x8018],manager,scene,m68k_get_reg(16),willowMotionLong(sram+0xf004),willowMotionLong(sram+0xf008),willowMotionLong(sram+0xf014),sourceRAM[0x82a2],sourceRAM[0x8293],routeTicks);fflush(stdout);
 }
 fclose(f);fclose(irqFile);f=outputFile("pointer-sites.csv","w");if(!f)return 2;fputs("address,source_store_pc\n",f);for(unsigned n=0;n<65536;n++)if(accepted[n])fprintf(f,"%04x,%x\n",n,accepted[n]);fclose(f);
 f=outputFile("irq-ring.bin","wb");if(!f||fwrite(sram+0xf000,1,0x800,f)!=0x800)return 2;fclose(f);
 printf("Continuous source cadence: %u paired physicalGenesis frames; full sourceRAM/sevenvideo/ports/registers/nativeIRQobservations and residualcycles match. No host CPU register writes or source wait/scheduler replacements. Proven pointer pairs%u, D0 residues%u, surviving saved-pointer bytes%u; source port reads/writes%u/%u and video accesses%u. No displayed latch/renderer or CPSspeed equivalence.\n",frames,pointerPairs,d0Residues,partialPointerBytes,portsRead,portsWritten,videoAccesses);
 if(route){
  printf("Coin/start route: startFrame%u, %u after-start active hardware frames, %u controlled-route ticks; source credits/input history/gameplay own all transitions. Primary manager%u, pending command%u\n",startFrame,activeFrames,routeTicks,willowMotionWord(sourceRAM+0x8000),willowMotionWord(sourceRAM+0x82c0));
  for(unsigned n=0;n<16;n++){unsigned at=n*32,usp=willowMotionLong(sourceRAM+at+8)&65535;printf("Task%02x state%u sleep%u pc%x usp%x return%x\n",at,sourceRAM[at],sourceRAM[at+1],willowMotionLong(sourceRAM+at+4),usp,usp<=65472?willowMotionLong(sourceRAM+usp+60):0);}
 }
 free(states[0]);free(states[1]);return 0;
}
