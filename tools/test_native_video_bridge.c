/* Native frame bridge vs the existing mapped original-instruction oracle.
 * Standalone fixture; no runtime dependencies on the portable gameplay path. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_boot_options.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_native_video_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],source[0x100000],expected[65536];

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
static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static uint16_t rw(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t rb(uint16_t at){return expected[at];}
static void ww(uint16_t at,uint16_t value){willowMotionPutWord(expected+at,value);}
static void wb(uint16_t at,uint8_t value){expected[at]=value;}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint16_t romWord(uint32_t at){if(at>=sizeof source-1)abort();return willowMotionWord(source+at);}
static uint32_t oracleLong(const uint8_t*ram,unsigned at){uint32_t v=0;for(unsigned n=0;n<4;n++)v=(v<<8)|ram[((at+n)&65535)^1];return v;}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} ForestCoreCycles;
static unsigned sym(const char*wanted){
 FILE*f=fopen("build/native-video-relocation/bridge.sym","r");if(!f)return 0;
 unsigned at,result=0;char kind,name[128];
 while(fscanf(f,"%x %c %127s",&at,&kind,name)==3)if(!strcmp(name,wanted))result=at;
 fclose(f);return result;
}
static uint8_t programImages[2][0x100000],cps[0x20000],phaseVideo[3][0x20000];
static unsigned pointerSites[65536],pointerPairs;
static uint32_t provenValues[65536],provenPC[65536];
static uint32_t phaseProvenValues[3][65536],phaseProvenPC[3][65536],acceptedPC[65536];
static unsigned d0Residues;
static struct {uint32_t source,target,size;} videoRegions[]={
 {0x910000,WILLOW_VIDEO_SCROLL1,0x4000},{0x918000,WILLOW_VIDEO_SCROLL2,0x4000},
 {0x914000,WILLOW_VIDEO_SCROLL3,0x4000},{0x900000,WILLOW_VIDEO_OBJECT0,0x800},
 {0x908000,WILLOW_VIDEO_OBJECT1,0x800},{0x91c000,WILLOW_VIDEO_PALETTES,0x1800}};
/* Exact relocation pairs include one-past-end cursors. RAM pairs are
 * admitted only with the original instruction-store provenance below. */
static int sameVideoPointer(unsigned original,unsigned native){
 for(unsigned n=0;n<6;n++)if(original>=videoRegions[n].source&&original<=videoRegions[n].source+videoRegions[n].size&&native==videoRegions[n].target+original-videoRegions[n].source)return 1;
 return 0;
}
static int sameValue(unsigned original,unsigned native){return original==native||sameVideoPointer(original,native);}
static int sameRegister(unsigned reg,unsigned original,unsigned native,unsigned continuation){
 if(sameValue(original,native))return 1;
 /* 3B57A MOVE.L A0,D0 then9324/1E9A/1EBC MOVE.W #1,D0: only the low word
  * changes. The original90xxxx/packed20xxxx upper word remains in D0. */
 if(reg==0&&(continuation==0x932c||continuation==0x1ea2||continuation==0x1ec4)&&original==0x00900001&&native==0x00200001){d0Residues++;return 1;}
 return 0;
}
/* Provenance only: observe the ORIGINAL native store before it executes.
 * Do not infer pointer ownership by scanning equal-looking scalar values. */
static void rememberPointerStores(unsigned at,unsigned(*get)(int)){
 if(at==0x1750||at==0x17a4||at==0x17c0){
  if(willowMotionLong(source+at)!=0x48e7fffe)abort();
  unsigned sp=get(15);if(sp<0xff103c||sp>0xff2000)abort();
  for(unsigned n=0;n<15;n++){unsigned address=(sp-60+n*4)&65535;provenValues[address]=get(n);provenPC[address]=at;}
 }else if(at==0xc42||at==0x127e){
  if(willowMotionLong(source+at)!=0x2d490030)abort();
  unsigned record=get(14);if(record<0xfff042||record>=0xfff442||(record-0xfff042)%64)abort();
  unsigned address=(record+48)&65535;provenValues[address]=get(9);provenPC[address]=at;
 }
}
static void validCPSAccess(unsigned at,unsigned width){
 for(unsigned n=0;n<6;n++)if(at>=videoRegions[n].source&&at+width<=videoRegions[n].source+videoRegions[n].size)return;
 fprintf(stderr,"Unrepresented original CPS video access %x width%u\n",at,width);abort();
}
static unsigned cpsRead8(unsigned at){if(at<0x900000||at>=0x920000)abort();validCPSAccess(at,1);return cps[at-0x900000];}
static unsigned cpsRead16(unsigned at){if(at<0x900000||at>=0x91ffff||(at&1))abort();validCPSAccess(at,2);return willowMotionWord(cps+(at-0x900000));}
static void cpsWrite8(unsigned at,unsigned v){if(at<0x900000||at>=0x920000)abort();validCPSAccess(at,1);cps[at-0x900000]=v;}
static void cpsWrite16(unsigned at,unsigned v){if(at<0x900000||at>=0x91ffff||(at&1))abort();validCPSAccess(at,2);willowMotionPutWord(cps+(at-0x900000),v);}
static unsigned char savedMaps[2*sizeof(((ForestCoreCycles*)0)->map[0])];
static void selectProgram(ForestCoreCycles*cpu,unsigned target){
 for(unsigned page=0;page<16;page++){
  uint8_t*base=cpu->map[page].base;if(!base)abort();
  for(unsigned n=0;n<65536;n++)base[n^1]=programImages[target][page*65536+n];
 }
 memcpy(cpu->map+0x90,savedMaps,sizeof savedMaps);
 if(!target)for(unsigned page=0x90;page<=0x91;page++){
  cpu->map[page].base=cps+(page-0x90)*65536;
  cpu->map[page].read8=cpsRead8;cpu->map[page].read16=cpsRead16;
  cpu->map[page].write8=cpsWrite8;cpu->map[page].write16=cpsWrite16;
 }
}
static int prepareBridgeROM(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(source,1,sizeof source,f)!=sizeof source)return 2;fclose(f);memcpy(programImages[0],source,sizeof source);
 f=fopen("build/native-video-relocation/bridge-program-input.bin","rb");if(!f||fread(programImages[1],1,sizeof source,f)!=sizeof source)return 2;fclose(f);
 for(unsigned n=0;n<2;n++){
  uint8_t*p=programImages[n];willowMotionPutLong(p,0xffff00);willowMotionPutLong(p+4,0x100000);
  memcpy(p+0x100,"SEGA GENESIS    ",16);memcpy(p+0x1f0,"U               ",16);
  const unsigned waits[]={0x1ea0,0x1ec2,0x932a};
  for(unsigned j=0;j<3;j++){if(willowMotionWord(p+waits[j])!=0x1750)return 2;willowMotionPutWord(p+waits[j],0x300);}
  willowMotionPutWord(p+0x300,0x4ef9);willowMotionPutWord(p+0x14cc,0x4ef9);willowMotionPutWord(p+0x15b4,0x4e75);
  /* Existing CPU fixture's task termination trap; physical IRQ not integrated. */
  willowMotionPutLong(p+0xa0,0x1638);
  p[0x1b0]='R';p[0x1b1]='A';p[0x1b2]=0xf8;p[0x1b3]=0x20;willowMotionPutLong(p+0x1b4,0x200000);willowMotionPutLong(p+0x1b8,0x20ffff);
 }
 memcpy(rom,programImages[0],sizeof source);willowMotionPutWord(rom+0x100000,0x60fe);return 0;
}
static uint8_t sourceRam[65536],bridgeRam[65536],phases[3][65536];
static unsigned phaseRegs[3][20],sourceRegisters[15];
static uint32_t ramLong(const uint8_t*p,unsigned at){return willowMotionLong(p+at);}
static void putRegRAM(uint8_t*ram,const uint8_t*values){for(unsigned n=0;n<65536;n++)ram[n^1]=values[n];}
static void getRegRAM(uint8_t*values,const uint8_t*ram){for(unsigned n=0;n<65536;n++)values[n]=ram[n^1];}
/* Only the artificial wake RTS target differs. Coroutine contexts, entire
 * allocated task stacks, main stack and all other source RAM are compared. */
static int bridgeTransient(unsigned n){return n>=0xf000&&n<0xf004;}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 unsigned ticks=argc==3?(unsigned)strtoul(argv[2],NULL,10):4096;
 if(!ticks||ticks>200000)return 2;
 if(prepareBridgeROM())return 2;
 unsigned entry=sym("nativeFrameBridgeProbe"),waitGate=sym("willowBridgeWaitGate"),taskGate=sym("willowBridgeSchedulerGate");
 unsigned cp[]={sym("willowBridgeMainCheckpoint"),sym("willowBridgeTaskCheckpoint"),sym("willowBridgeWakeCheckpoint")};
 unsigned context=sym("willowBridgeMainRegisters")-0x200000,mainPC=sym("willowBridgeMainPC")-0x200000;
 unsigned mainSR=sym("willowBridgeMainSR")-0x200000;
 unsigned mainSP=sym("willowBridgeMainSP")-0x200000,gameUSP=sym("willowBridgeGameUSP")-0x200000,mode=sym("willowBridgeMode")-0x200000;
 if(entry<0x100010||waitGate<0x100010||taskGate<0x100010||mode<0xf000||mode>=0xf800)return 2;
 FILE*f=fopen("build/native-video-relocation/bridge.bin","rb");if(!f)return 2;
 size_t bytes=fread(rom+0x100010,1,0xffff0,f);if(!bytes||!feof(f))return 2;fclose(f);
 for(unsigned n=0;n<2;n++){willowMotionPutLong(programImages[n]+0x302,waitGate);willowMotionPutLong(programImages[n]+0x14ce,taskGate);}
 memcpy(rom,programImages[0],sizeof source);
 willowMotionPutWord(rom+0x10000e,0x4e71);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 f=fopen("build/native-video-relocation/bridge-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-video-relocation/bridge-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");ForestCoreCycles*cpu=dlsym(core,"m68k");if(!ram||!cpu)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,sym("willowBridgeEnableSRAM"));retro_run();
 if(m68k_get_reg(16)!=0x100000)return 2;
 memcpy(savedMaps,cpu->map+0x90,sizeof savedMaps);
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
#define TO_STOP() do{cpu->cycles=0;cpu->refresh=896;unsigned instructions=0;while(m68k_get_reg(16)!=0x100000&&instructions++<4000000){unsigned at=m68k_get_reg(16);rememberPointerStores(at,m68k_get_reg);m68k_run(cpu->cycles+1);}if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle failed at PC%x\n",m68k_get_reg(16));return 1;}}while(0)
 /* Verify the core's USP register API against real MOVE to USP, not layout guesses. */
 m68k_set_reg(8,0xff1234);m68k_set_reg(15,0xffff00);m68k_set_reg(16,sym("willowBridgeSetUSP"));TO_STOP();
 if(m68k_get_reg(19)!=0xff1234)return 2;m68k_set_reg(19,0xff4320);
 if(m68k_get_reg(19)!=0xff4320)return 2;
 for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7);
 wb(0xf9a3,5);wb(0x8288,3);ww(0x8000,0);ww(0x8004,0);ww(0x8008,0);
 wb(0x8058,0);wb(0x8068,0);ww(0x8070,10000);wb(0x82a0,0);wb(0x8023,0);
 wb(0x8067,0);ww(0x828a,0);ww(0x828c,0x183c);ww(0x801a,0);ww(0x801e,0);wb(0x8290,0);wb(0x8293,0);
 memset(expected,0,512);wb(0x020e,0);wb(0x828f,0);willowArcadeTaskInitFree(&bus,romWord);
 for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x1100+n*256);}
 for(unsigned n=0;n<128;n++)ww(0x8072+n*2,65535);
 ww(0x801c,0);willowArcadeTaskStart(&bus,0x60,0x1ddc,0,0,1);
 ww(0x82c0,0);wb(0xf890,0);wb(0xf891,0);for(unsigned n=0;n<12;n++)ww(0x82a8+n*2,0);
 for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,(uint16_t)((n/16)%16)*0x1000|((n*17)&4095));
 for(uint16_t at=0x8000;at<0x8014;at+=2)ww(at,0);
 wb(0x8293,0);wb(0xf99f,0);wb(0x82a3,0);wb(0x8066,0);wb(0x8067,0);wb(0x8068,0);wb(0xf99e,0);
 if(!willowArcadeBootOptions(&bus,0x1ac2,romWord)||!willowArcadeBootOptions(&bus,0x1b46,romWord)||!willowArcadeBootOptions(&bus,0x1b96,romWord))return 2;
 memcpy(sourceRam,expected,65536);memcpy(bridgeRam,expected,65536);
 memset(sram,0xa5,65536);memset(cps,0xa5,sizeof cps);
 for(unsigned n=0;n<3072;n++)willowMotionPutWord(cps+0x1c000+n*2,(uint16_t)((n/16)%16)*0x1000|((n*17)&4095));
 for(unsigned region=0;region<6;region++)memcpy(sram+(videoRegions[region].target-0x200000),cps+(videoRegions[region].source-0x900000),videoRegions[region].size);
 memset(sram+0xf000,0,0x800);memset(sram+0xfe00,0xa5,512);
 uint32_t pc=0x92d2,sp=0xffff00,usp=0;sourceRegisters[12]=0x123456;sourceRegisters[13]=0xff8000;
 for(unsigned n=0;n<15;n++)willowMotionPutLong(sram+context+n*4,sourceRegisters[n]);
 willowMotionPutLong(sram+mainPC,pc);willowMotionPutLong(sram+mainSP,sp);willowMotionPutLong(sram+gameUSP,usp);
 unsigned nestedWaits=0,active=0,checks=0,taskSleepTraps=0,taskRTEs=0,userInstructions=0;
 unsigned mainDepth=0,schedulerDepth=0,taskDepth=0;unsigned long long sourceCycles=0,bridgeCycles=0;
 f=fopen("build/native-video-relocation/bridge-timing.csv","w");if(!f)return 2;fputs("pass,manager,scene,source_cycles,bridge_cycles\n",f);
 for(unsigned pass=0;pass<ticks;pass++){

  unsigned manager=ramLong(sourceRam,0x8004)>>16,scene=ramLong(sourceRam,0x8008)>>16;
  uint8_t keys[]={1,1,0x11,1,0x21,1,0x11,1};uint8_t held=(manager==2&&scene==2)?keys[(active/16)%8]:0;
  if(manager==2&&scene==2)active++;
  sourceRam[0x8060]=sourceRam[0x8062]=bridgeRam[0x8060]=bridgeRam[0x8062]=held;
  selectProgram(cpu,0);putRegRAM(ram,sourceRam);sram[mode]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(19,usp);for(unsigned n=0;n<15;n++)m68k_set_reg(n,sourceRegisters[n]);
  m68k_set_reg(15,sp);m68k_set_reg(16,pc);TO_STOP();unsigned originalCost=cpu->cycles;
  memcpy(phaseProvenValues[0],provenValues,sizeof provenValues);memcpy(phaseProvenPC[0],provenPC,sizeof provenPC);memcpy(phaseVideo[0],cps,sizeof cps);getRegRAM(phases[0],ram);for(unsigned n=0;n<20;n++)phaseRegs[0][n]=m68k_get_reg(n);
  for(unsigned n=0;n<15;n++)sourceRegisters[n]=m68k_get_reg(n);
  sp=m68k_get_reg(15);pc=ramLong(phases[0],sp&65535);sp+=4;
  if(pc!=0x932c)nestedWaits++;
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);TO_STOP();originalCost+=cpu->cycles;
  memcpy(phaseProvenValues[1],provenValues,sizeof provenValues);memcpy(phaseProvenPC[1],provenPC,sizeof provenPC);memcpy(phaseVideo[1],cps,sizeof cps);getRegRAM(phases[1],ram);for(unsigned n=0;n<20;n++)phaseRegs[1][n]=m68k_get_reg(n);
  putRamWord(ram,0xf000,0x10);putRamWord(ram,0xf002,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);TO_STOP();originalCost+=cpu->cycles;
  memcpy(phaseProvenValues[2],provenValues,sizeof provenValues);memcpy(phaseProvenPC[2],provenPC,sizeof provenPC);memcpy(phaseVideo[2],cps,sizeof cps);getRegRAM(phases[2],ram);for(unsigned n=0;n<20;n++)phaseRegs[2][n]=m68k_get_reg(n);
  memcpy(sourceRam,phases[2],65536);usp=m68k_get_reg(19);
  selectProgram(cpu,1);putRegRAM(ram,bridgeRam);sram[mode]=1;
  unsigned caller[15];for(unsigned n=0;n<15;n++){caller[n]=0x51710000+n*0x1111;m68k_set_reg(n,caller[n]);}
  m68k_set_reg(17,0x2715);m68k_set_reg(19,0xfffab0);m68k_set_reg(15,0x20fff0);willowMotionPutLong(sram+0xfff0,0x100000);m68k_set_reg(16,entry);
  cpu->cycles=0;cpu->refresh=896;unsigned instructions=0,phase=0;
  /* Only READ CPU/RAM inside this call; no host register/memory mutations. */
  while(m68k_get_reg(16)!=0x100000&&instructions++<12000000){
   if(phase<3&&m68k_get_reg(16)==cp[phase]){
    for(unsigned n=0;n<65536;n++)if(!bridgeTransient(n)){
     if(!(n&1)&&n<=65532){
      unsigned original=willowMotionLong(phases[phase]+n),native=oracleLong(ram,n);
      if(original!=native&&sameVideoPointer(original,native)){
       if(!phaseProvenPC[phase][n]||phaseProvenValues[phase][n]!=original){fprintf(stderr,"UNPROVEN pointer pair pass%u phase%u RAM%x source%x target%x laststore%x lastvalue%x\n",pass,phase,n,original,native,phaseProvenPC[phase][n],phaseProvenValues[phase][n]);return 1;}
       if(!pointerSites[n])fprintf(stderr,"Proven pointer site pass%u phase%u RAM%x source%x target%x native-store-PC%x\n",pass,phase,n,original,native,phaseProvenPC[phase][n]);
       acceptedPC[n]=phaseProvenPC[phase][n];pointerSites[n]++;pointerPairs++;n+=3;continue;
      }
     }
     if(ram[n^1]!=phases[phase][n]){fprintf(stderr,"All-video RAM mismatch pass%u phase%u address%x got%x expected%x\n",pass,phase,n,ram[n^1],phases[phase][n]);return 1;}
    }
    for(unsigned region=0;region<6;region++)for(unsigned n=0;n<videoRegions[region].size;n++){
     unsigned actual=sram[videoRegions[region].target-0x200000+n],wanted=phaseVideo[phase][videoRegions[region].source-0x900000+n];
     if(actual!=wanted){fprintf(stderr,"All-video mismatch pass%u phase%u region%u offset%x got%x expected%x\n",pass,phase,region,n,actual,wanted);return 1;}
    }
    for(unsigned n=0xc800;n<0xd000;n++)if(sram[n]!=0xa5){fprintf(stderr,"Unused displayed-object latch changed pass%u at%x\n",pass,n);return 1;}
    for(unsigned n=0;n<20;n++)if(n!=16&&!sameRegister(n,phaseRegs[phase][n],m68k_get_reg(n),phase==0?pc:0)){
     fprintf(stderr,"Bridge register mismatch pass%u phase%u reg%u got%x expected%x continuation%x\n",pass,phase,n,m68k_get_reg(n),phaseRegs[phase][n],pc);return 1;
    }
    phase++;checks++;
   }
   unsigned address=m68k_get_reg(16),status=m68k_get_reg(17),stack=m68k_get_reg(15);
   if(address<0x100000&&phase<3){
    if(!(status&0x2000)){
     if(phase!=1){fprintf(stderr,"Unexpected user task phase%u PC%x\n",phase,address);return 1;}
     unsigned slot=oracleLong(ram,0x204);
     if(slot<0xff0000||slot>=0xff0200||(slot&31))return 1;
     unsigned top=oracleLong(ram,(slot&65535)+12);
     if(top<0xff1100||top>0xff2000||(top&255)||stack>top||stack<top-256){fprintf(stderr,"Task stack escaped slot%x SP%x top%x PC%x\n",slot,stack,top,address);return 1;}
     if(top-stack>taskDepth)taskDepth=top-stack;userInstructions++;
    }else if(phase==0){
     if(stack<0xfffa00||stack>0xffff00){fprintf(stderr,"Main stack escaped SP%x PC%x\n",stack,address);return 1;}
     if(0xffff00-stack>mainDepth)mainDepth=0xffff00-stack;
    }else if(phase==1){
     if(stack<0xffef80||stack>0xfff000){fprintf(stderr,"Scheduler stack escaped SP%x PC%x\n",stack,address);return 1;}
     if(0xfff000-stack>schedulerDepth)schedulerDepth=0xfff000-stack;
     if(address==0x175c)taskSleepTraps++;
     if(willowMotionWord(rom+address)==0x4e73)taskRTEs++;
    }else if(stack!=0xfff000){fprintf(stderr,"Wake stack escaped SP%x PC%x\n",stack,address);return 1;}
   }
   m68k_run(cpu->cycles+1);
  }
  if(m68k_get_reg(16)!=0x100000||phase!=3){fprintf(stderr,"Bridge failed pass%u pc%x phase%u\n",pass,m68k_get_reg(16),phase);return 1;}
  for(unsigned n=0;n<15;n++)if(m68k_get_reg(n)!=caller[n]){fprintf(stderr,"Caller reg%u not restored\n",n);return 1;}
  if(m68k_get_reg(15)!=0x20fff4||m68k_get_reg(17)!=0x2715||m68k_get_reg(19)!=0xfffab0)return 1;
  for(unsigned n=0;n<15;n++)if(!sameRegister(n,sourceRegisters[n],willowMotionLong(sram+context+n*4),pc))return 1;
  if(willowMotionWord(sram+mainSR)!=phaseRegs[0][17])return 1;
  if(willowMotionLong(sram+mainPC)!=pc||willowMotionLong(sram+mainSP)!=sp||willowMotionLong(sram+gameUSP)!=usp)return 1;
  for(unsigned n=0xfe00;n<0x10000;n++)if((n<0xffe8||n>=0xfff4)&&sram[n]!=0xa5){fprintf(stderr,"Caller stack canary changed at%x\n",n);return 1;}
  getRegRAM(bridgeRam,ram);sourceCycles+=originalCost;bridgeCycles+=cpu->cycles;
  fprintf(f,"%u,%u,%u,%u,%u\n",pass,willowMotionWord(sourceRam+0x8004),willowMotionWord(sourceRam+0x8008),originalCost,cpu->cycles);
  if(!(pass%256)){printf("Native bridge pass%u state%u/%u sourcePC%x active%u nested%u\n",pass,willowMotionWord(sourceRam+0x8004),willowMotionWord(sourceRam+0x8008),pc,active,nestedWaits);fflush(stdout);}
 }
 fclose(f);
 if(!taskSleepTraps||!taskRTEs||!userInstructions||(ticks>=2048&&(!active||!nestedWaits)))return 1;
 printf("Source task execution: %u sleep traps, %u RTEs, %u user instructions; maximum main/scheduler/task stack depths %u/%u/%u bytes\n",taskSleepTraps,taskRTEs,userInstructions,mainDepth,schedulerDepth,taskDepth);
 FILE*sites=fopen("build/native-video-relocation/pointer-sites.csv","w");if(!sites)return 2;fputs("address,comparisons,source_store_pc\n",sites);for(unsigned n=0;n<65536;n++)if(pointerSites[n])fprintf(sites,"%04x,%u,%x\n",n,pointerSites[n],acceptedPC[n]);fclose(sites);
 printf("All-video parity: %u source-store-proven pointer pairs accepted; %u source-proven D0 partial-word residues\n",pointerPairs,d0Residues);
 printf("Native main/task/wake bridge: %u one-call updates, %u full-RAM/register checkpoints, %u active gameplay updates, %u nested waits; source/coroutine stacks modulo proven pointer relocation, C ABI, SR/SP/USP and persistent context match. Mean source/bridge master cycles %.3f/%.3f\n",ticks,checks,active,nestedWaits,(double)sourceCycles/ticks,(double)bridgeCycles/ticks);
 return 0;
}
