/* Native frame bridge vs the existing mapped original-instruction oracle.
 * Standalone fixture; no runtime dependencies on the portable gameplay path. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_boot_options.h"
#include "../src/willow_arcade_tasks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],source[0x100000],expected[65536];
static uint32_t videoBase;static unsigned videoSize;
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
 FILE*f=fopen("build/native-frame-bridge/bridge.sym","r");if(!f)return 0;
 unsigned at,result=0;char kind,name[128];
 while(fscanf(f,"%x %c %127s",&at,&kind,name)==3)if(!strcmp(name,wanted))result=at;
 fclose(f);return result;
}
static int prepareBridgeROM(void){
 unsigned region=1;videoBase=0x918000;videoSize=0x4000;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);

 /* Intercept only the outer manager wait; scheduled tasks use real 1750. */
 if(willowMotionWord(rom+0x1ea0)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ea0,0x300);
 if(willowMotionWord(rom+0x1ec2)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ec2,0x300);
 if(willowMotionWord(rom+0x932a)!=0x1750)return 2;
 willowMotionPutWord(rom+0x932a,0x300);
 willowMotionPutWord(rom+0x300,0x4ef9);willowMotionPutLong(rom+0x302,0x100000);
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutLong(rom+0xa8,0x15f2);willowMotionPutLong(rom+0x88,0x1690);
 if(region==1)for(unsigned n=0;n<12;n++){
  uint32_t at=0x340b6+n*4,original=willowMotionLong(rom+at);
  if(original<0x918000||original>=0x91c000)return 2;
  willowMotionPutLong(rom+at,0xff4000+original-0x918000);
 }
 /* 32E58 stores an absolute Scroll2 pointer before the first transition
  * write. Relocate that literal as well as retained record pointers. */
 if(willowMotionLong(rom+0x32e5a)!=0x9197dc)return 2;
 if(region==1)willowMotionPutLong(rom+0x32e5a,0xff57dc);
 /* 31FB8 computes the paired floor attributes from the Scroll2 base. */
 if(willowMotionLong(rom+0x31fba)!=0x918000)return 2;
 if(region==1)willowMotionPutLong(rom+0x31fba,0xff4000);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 if(willowMotionLong(rom+0x679e)!=0x910034)return 2;
 if(region==0)willowMotionPutLong(rom+0x679e,0xff4034);
 if(willowMotionLong(rom+0xb38)!=0x91c800||willowMotionLong(rom+0xb4a)!=0x91cc00)return 2;
 willowMotionPutLong(rom+0xb38,0xff2800);willowMotionPutLong(rom+0xb4a,0xff2c00);
 if(region>=3){
  unsigned at=region==3?0x6620:0x662a;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 const unsigned clears[]={0x5876,0x588a,0x589e,0x58bc,0x58c6};
 if(willowMotionLong(rom+clears[region])!=videoBase)return 2;
 willowMotionPutLong(rom+clears[region],0xff4000);
 const unsigned layerClears[]={0x5a14,0x5a50,0x5a90,0x59d8,0x59e2};
 if(willowMotionLong(rom+layerClears[region])!=videoBase)return 2;
 willowMotionPutLong(rom+layerClears[region],0xff4000);
 if(region<3){
  const unsigned resetClears[]={0x6644,0x6656,0x6668};
  if(willowMotionLong(rom+resetClears[region])!=videoBase)return 2;
  willowMotionPutLong(rom+resetClears[region],0xff4000);
 }
 const unsigned fadePatches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  if(willowMotionLong(rom+fadePatches[n])!=0x91c000+n*1024)return 2;
  willowMotionPutLong(rom+fadePatches[n],0xff2000+n*1024);
 }
 const unsigned bankFadePatches[]={0x531c,0x5346,0x5370,0x539a,0x53c2,0x53f0,0x541e,0x544c};
 for(unsigned n=0;n<8;n++){
  if(willowMotionLong(rom+bankFadePatches[n])!=0x91c000+(n%4)*1024)return 2;
  willowMotionPutLong(rom+bankFadePatches[n],0xff2000+(n%4)*1024);
 }
 /* Bank-one relocation changes its identifying address bit from 15 to 14. */
 if(region==4){
  if(willowMotionWord(rom+0x3b57e)!=15)return 2;
  willowMotionPutWord(rom+0x3b57e,14);
 }
 if(region>=3){
  unsigned spriteAt=region==3?0x3b538:0x3b54c;
  if(willowMotionLong(rom+spriteAt)!=videoBase)return 2;
  willowMotionPutLong(rom+spriteAt,0xff4000);
  unsigned at=region==3?0x58f8:0x5902;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 const unsigned palettePatches[]={0xae4,0xb06,0xaf6,0xb16};
 const uint32_t paletteTargets[]={0x91c800,0x91cc00,0x91c400,0x91c200};
 for(unsigned n=0;n<4;n++){
  if(willowMotionLong(rom+palettePatches[n])!=paletteTargets[n])return 2;
  willowMotionPutLong(rom+palettePatches[n],paletteTargets[n]-0x91c000+0xff2000);
 }
 if(region==1||region==2){
  unsigned at=region==1?0x4365a:0x43d50;
  if(willowMotionLong(rom+at)!=videoBase)return 2;
  willowMotionPutLong(rom+at,0xff4000);
 }
 /* This standalone fixture always owns the live Scroll2 collision map. */
 if(willowMotionLong(rom+0x3e44)!=0x918000)return 2;
 willowMotionPutLong(rom+0x3e44,0xff4000);
 /* Relocate complete stage palettes and all HUD/message output paths. */
 const unsigned colors[]={0x988,0x99a,0x9b0,0x9cc,0x9da,0xc242,0xea68,0xc2e,0x1276};
 const uint32_t colorBases[]={0x91c000,0x91c200,0x91c800,0x91c400,0x91cc00,0x91c000,0x91c0e0,0x91c000,0x91c000};
 for(unsigned n=0;n<sizeof colors/sizeof colors[0];n++){
  if(willowMotionLong(rom+colors[n])!=colorBases[n])return 2;
  willowMotionPutLong(rom+colors[n],0xff2000+colorBases[n]-0x91c000);
 }
 willowMotionPutLong(rom+0x88,0x1690);willowMotionPutLong(rom+0xa0,0x1638);
 if(region==0){
 if(willowMotionLong(rom+0x7d74)!=0x910000)return 2;
 willowMotionPutLong(rom+0x7d74,0xff4000);
 const unsigned patches[]={0x7d84,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910200,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}

  willowMotionPutLong(rom+0x5c32,0xff4000);
  willowMotionPutLong(rom+0x4b72,0xff4170);
  for(unsigned n=0;n<47;n++){
   unsigned at=0x7a1e + n*4;
   willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
  }
  for(unsigned n=0;n<5;n++)for(unsigned t=0;t<2;t++){
   unsigned at=(t?0x7c64:0x7c50)+n*4;
   willowMotionPutLong(rom+at,willowMotionLong(rom+at)-0x910000+0xff4000);
  }
 }
 memcpy(rom+0x300000,source,sizeof source);
 return 0;
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
 if(entry<0x100010||waitGate<0x100010||taskGate<0x100010||mode>=4096)return 2;
 FILE*f=fopen("build/native-frame-bridge/bridge.bin","rb");if(!f)return 2;
 size_t bytes=fread(rom+0x100010,1,0xffff0,f);if(!bytes||!feof(f))return 2;fclose(f);
 willowMotionPutLong(rom+0x302,waitGate);willowMotionPutLong(rom+0x14ce,taskGate);
 willowMotionPutWord(rom+0x10000e,0x4e71);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 f=fopen("build/native-frame-bridge/oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-frame-bridge/oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");ForestCoreCycles*cpu=dlsym(core,"m68k");if(!ram||!cpu)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,sym("willowBridgeEnableSRAM"));retro_run();
 if(m68k_get_reg(16)!=0x100000)return 2;
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
#define TO_STOP() do{cpu->cycles=0;cpu->refresh=896;unsigned instructions=0;while(m68k_get_reg(16)!=0x100000&&instructions++<4000000)m68k_run(cpu->cycles+1);if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Oracle failed at PC%x\n",m68k_get_reg(16));return 1;}}while(0)
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
 memset(sram,0,4096);memset(sram+0xfe00,0xa5,512);
 uint32_t pc=0x92d2,sp=0xffff00,usp=0;sourceRegisters[12]=0x123456;sourceRegisters[13]=0xff8000;
 for(unsigned n=0;n<15;n++)willowMotionPutLong(sram+context+n*4,sourceRegisters[n]);
 willowMotionPutLong(sram+mainPC,pc);willowMotionPutLong(sram+mainSP,sp);willowMotionPutLong(sram+gameUSP,usp);
 unsigned nestedWaits=0,active=0,checks=0,taskSleepTraps=0,taskRTEs=0,userInstructions=0;
 unsigned mainDepth=0,schedulerDepth=0,taskDepth=0;unsigned long long sourceCycles=0,bridgeCycles=0;
 f=fopen("build/native-frame-bridge/timing.csv","w");if(!f)return 2;fputs("pass,manager,scene,source_cycles,bridge_cycles\n",f);
 for(unsigned pass=0;pass<ticks;pass++){
  unsigned manager=ramLong(sourceRam,0x8004)>>16,scene=ramLong(sourceRam,0x8008)>>16;
  uint8_t keys[]={1,1,0x11,1,0x21,1,0x11,1};uint8_t held=(manager==2&&scene==2)?keys[(active/16)%8]:0;
  if(manager==2&&scene==2)active++;
  sourceRam[0x8060]=sourceRam[0x8062]=bridgeRam[0x8060]=bridgeRam[0x8062]=held;
  putRegRAM(ram,sourceRam);sram[mode]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(19,usp);for(unsigned n=0;n<15;n++)m68k_set_reg(n,sourceRegisters[n]);
  m68k_set_reg(15,sp);m68k_set_reg(16,pc);TO_STOP();unsigned originalCost=cpu->cycles;
  getRegRAM(phases[0],ram);for(unsigned n=0;n<20;n++)phaseRegs[0][n]=m68k_get_reg(n);
  for(unsigned n=0;n<15;n++)sourceRegisters[n]=m68k_get_reg(n);
  sp=m68k_get_reg(15);pc=ramLong(phases[0],sp&65535);sp+=4;
  if(pc!=0x932c)nestedWaits++;
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);TO_STOP();originalCost+=cpu->cycles;
  getRegRAM(phases[1],ram);for(unsigned n=0;n<20;n++)phaseRegs[1][n]=m68k_get_reg(n);
  putRamWord(ram,0xf000,0x10);putRamWord(ram,0xf002,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);TO_STOP();originalCost+=cpu->cycles;
  getRegRAM(phases[2],ram);for(unsigned n=0;n<20;n++)phaseRegs[2][n]=m68k_get_reg(n);
  memcpy(sourceRam,phases[2],65536);usp=m68k_get_reg(19);
  putRegRAM(ram,bridgeRam);sram[mode]=1;
  unsigned caller[15];for(unsigned n=0;n<15;n++){caller[n]=0x51710000+n*0x1111;m68k_set_reg(n,caller[n]);}
  m68k_set_reg(17,0x2715);m68k_set_reg(19,0xfffab0);m68k_set_reg(15,0x20fff0);willowMotionPutLong(sram+0xfff0,0x100000);m68k_set_reg(16,entry);
  cpu->cycles=0;cpu->refresh=896;unsigned instructions=0,phase=0;
  /* Only READ CPU/RAM inside this call; no host register/memory mutations. */
  while(m68k_get_reg(16)!=0x100000&&instructions++<12000000){
   if(phase<3&&m68k_get_reg(16)==cp[phase]){
    for(unsigned n=0;n<65536;n++)if(!bridgeTransient(n)&&ram[n^1]!=phases[phase][n]){
     fprintf(stderr,"Bridge RAM mismatch pass%u phase%u address%x got%x expected%x\n",pass,phase,n,ram[n^1],phases[phase][n]);return 1;
    }
    for(unsigned n=0;n<20;n++)if(n!=16&&m68k_get_reg(n)!=phaseRegs[phase][n]){
     fprintf(stderr,"Bridge register mismatch pass%u phase%u reg%u got%x expected%x\n",pass,phase,n,m68k_get_reg(n),phaseRegs[phase][n]);return 1;
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
  for(unsigned n=0;n<15;n++)if(willowMotionLong(sram+context+n*4)!=sourceRegisters[n])return 1;
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
 printf("Native main/task/wake bridge: %u one-call updates, %u full-RAM/register checkpoints, %u active gameplay updates, %u nested waits; source/coroutine stacks, C ABI, SR/SP/USP and persistent context match. Mean source/bridge master cycles %.3f/%.3f\n",ticks,checks,active,nestedWaits,(double)sourceCycles/ticks,(double)bridgeCycles/ticks);
 return 0;
}
