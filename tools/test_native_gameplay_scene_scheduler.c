/* Compiled initial main task with actual command consumption, fade tasks and wakes.
 * One manager boundary and scheduler pass per fixture tick; no forced completion. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_native_task_runtime.h"
#include "../src/willow_native_gameplay_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],source[0x100000];
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT || cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static uint8_t expected[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint8_t nativeInput[65536],oracleAfter[65536];
static uint32_t videoBase;static unsigned videoSize;
static unsigned videoWrites,paletteWrites;
static uint16_t romWord(uint32_t at){return willowMotionWord(source+at);}
static void videoWord(uint32_t at,uint16_t v){if(at>=videoBase&&at<videoBase+videoSize){ww(0x4000+at-videoBase,v);videoWrites++;}}
static uint8_t*resolve(uint16_t at){return expected+at;}
static uint8_t romByte(uint32_t at){return source[at];}
static void write2(uint16_t at,uint16_t value){videoWord(0x918000UL+at,value);}
static void write3(uint16_t at,uint16_t value){videoWord(0x914000UL+at,value);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d800)abort();ww(0x2000+at-0x91c000,v);paletteWrites++;}
static uint16_t colorRead(uint32_t at){return rw(0x2000+at-0x91c000);}
static int cpuContext(unsigned a){return (a<512&&a%32>=2&&a%32<12)||(a>=0xd4&&a<0xdc)||(a>=0x200&&a<0x204)||(a>=0x1000&&a<0x2000)||(a>=0xef80&&a<0xf004)||(a>=0xfe80&&a<0xff04);}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 unsigned region=strtoul(argv[2],0,10);if(region>4)return 2;
 const uint32_t videoBases[]={0x910000,0x918000,0x914000,0x900000,0x908000};
 videoBase=videoBases[region];videoSize=region<3?0x4000:0x800;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Intercept only the outer manager wait; scheduled tasks use real 1750. */
 if(willowMotionWord(rom+0x1ea0)!=0x1750)return 2;
 willowMotionPutWord(rom+0x1ea0,0x300);
 if(willowMotionWord(rom+0x932a)!=0x1750)return 2;
 willowMotionPutWord(rom+0x932a,0x300);
 willowMotionPutWord(rom+0x300,0x4ef9);willowMotionPutLong(rom+0x302,0x100000);
 willowMotionPutWord(rom+0x14cc,0x4ef9);willowMotionPutLong(rom+0x14ce,0x100000);
 willowMotionPutLong(rom+0x80,0x15d0);willowMotionPutLong(rom+0x9c,0x160e);
 willowMotionPutLong(rom+0x8c,0x175c);willowMotionPutLong(rom+0x90,0x1788);willowMotionPutLong(rom+0xa4,0x165c);
 willowMotionPutWord(rom+0x15b4,0x4e75);
 const unsigned clears[]={0x5876,0x588a,0x589e,0x58bc,0x58c6};
 if(willowMotionLong(rom+clears[region])!=videoBase)return 2;
 willowMotionPutLong(rom+clears[region],0xff4000);
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
 if(region>=3){
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
 memcpy(rom+0x300000,source,sizeof source);
 f=fopen("build/scene_runtime_probe.bin","rb");if(!f)return 2;
 size_t nativeSize=fread(rom+0x100010,1,0xffff0,f);if(!nativeSize||!feof(f))return 2;fclose(f);
 unsigned targetEntry=0,targetAddress[9]={0};
 const char*names[]={"timeoutProbeOperation","timeoutProbeRegion","timeoutProbeStatus","timeoutProbeD4","timeoutProbeA4","timeoutProbeWait","timeoutProbeFlow","timeoutProbeRuntime",0};
 f=fopen("build/scene_runtime_probe.sym","r");if(!f)return 2;
 unsigned address;char kind,name[128];
 while(fscanf(f,"%x %c %127s",&address,&kind,name)==3){
  if(!strcmp(name,"timeoutRuntimeProbe"))targetEntry=address;
  for(unsigned n=0;names[n];n++)if(!strcmp(name,names[n]))targetAddress[n]=address-0x200000;
 }
 fclose(f);if(targetEntry<0x100010||targetEntry>=0x200000)return 2;
 for(unsigned n=0;names[n];n++)if(targetAddress[n]<0x800||targetAddress[n]>0x1fc0)return 2;
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 willowMotionPutWord(rom+0x100004,0x13fc);willowMotionPutWord(rom+0x100006,1);
 willowMotionPutLong(rom+0x100008,0xa130f1);willowMotionPutWord(rom+0x10000c,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/native-scene-scheduler-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/native-scene-scheduler-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100004);retro_run();
 if(m68k_get_reg(16)!=0x100000)return 2;
 unsigned targetChecks=0;
#define TARGET_CHECK(operation,wanted) do{ \
 unsigned regs[18];for(unsigned i=0;i<18;i++)regs[i]=m68k_get_reg(i); \
 memcpy(oracleAfter,ram,65536); \
 for(unsigned i=0;i<65536;i++)ram[i^1]=nativeInput[i]; \
 willowMotionPutWord(sram+targetAddress[0],operation); \
 willowMotionPutWord(sram+targetAddress[2],0xdead); \
 willowMotionPutLong(sram+0xff00,0x100000); \
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0x20ff00);m68k_set_reg(16,targetEntry); \
 for(unsigned slice=0;slice<128&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000||willowMotionWord(sram+targetAddress[2])!=(wanted)){fprintf(stderr,"Target boundary failed op %u region %u sample %u pass %u PC %x status %x\n",operation,region,sample,pass,m68k_get_reg(16),willowMotionWord(sram+targetAddress[2]));return 1;} \
 if(m68k_get_reg(15)!=0x20ff04)return 1; \
 for(unsigned i=0xbf00;i<0xc000;i++)if(sram[i]!=0xa5)return 1; \
 for(unsigned i=0;i<65536;i++)if(ram[i^1]!=expected[i]){fprintf(stderr,"Target RAM op %u region %u sample %u pass %u at %x actual %x expected %x\n",operation,region,sample,pass,i,ram[i^1],expected[i]);return 1;} \
 if(willowMotionWord(sram+targetAddress[3])!=d4||willowMotionLong(sram+targetAddress[4])!=a4||willowMotionWord(sram+targetAddress[5])!=eventWait||willowMotionWord(sram+targetAddress[6])!=flow.task.pc||willowMotionWord(sram+targetAddress[6]+2)!=flow.manager.target||willowMotionWord(sram+targetAddress[6]+4)!=flow.entry.pc||willowMotionWord(sram+targetAddress[6]+6)!=flow.route.pc||willowMotionWord(sram+targetAddress[6]+8)!=flow.initialD1||willowMotionWord(sram+targetAddress[6]+10)!=flow.initialD1Known||willowMotionWord(sram+targetAddress[6]+12)!=flow.scene.pc||willowMotionWord(sram+targetAddress[6]+14)!=flow.scene.target)return 1; \
 for(unsigned i=0;i<16;i++)if(willowMotionWord(sram+targetAddress[7]+i*2)!=runtime.resume[i])return 1; \
 memcpy(ram,oracleAfter,65536);m68k_set_reg(17,regs[17]);for(unsigned i=0;i<17;i++)m68k_set_reg(i,regs[i]);targetChecks++; \
}while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,waits=0,completions=0;
 for(unsigned sample=0;sample<20;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31);
  ww(0x8000,0);ww(0x8004,0);ww(0x8008,0);
  wb(0x8058,0);wb(0x8068,0);ww(0x8070,3);
  wb(0x8067,((sample/2)%6)*32);ww(0x828a,0);ww(0x828c,0x183c);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);
  WillowNativeTaskRuntime runtime={0};
  memset(expected,0,512);wb(0x020e,0);wb(0x828f,sample&1);
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x1100+n*256);}
  for(unsigned n=0;n<128;n++)ww(0x8072+n*2,65535);
  ww(0x801c,rw(0x801a));willowArcadeTaskStart(&bus,0x60,0x1ddc,0,0,1);
  ww(0x82c0,0);wb(0xf890,0);wb(0xf891,0);
  for(unsigned n=0;n<12;n++)ww(0x82a8+n*2,0);
  for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,(uint16_t)(((n/16+sample)%16)*0x1000)|((n*17)&4095));
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t d4=sample*977,eventWait=0;uint32_t a4=0x123456;
  WillowNativeGameplayFlow flow={0};
  memset(sram+0x800,0,0x1800);memset(sram+0xbf00,0xa5,256);
  willowMotionPutWord(sram+targetAddress[1],region);
  willowMotionPutWord(sram+targetAddress[3],d4);willowMotionPutLong(sram+targetAddress[4],a4);
  WillowPaletteAnimation palette={0};palette.read=colorRead;palette.write=colorWrite;
  WillowArcadeGameplayServices services={.palette=&palette,.resolve=resolve,.byte=romByte,.write2=write2,.write3=write3,.bus=&bus,.d4=&d4,.a4=&a4,.eventWait=&eventWait,.busWord=romWord,.mapClearWrite=videoWord};
  WillowNativeGameplayFlowBinding binding={&flow,&bus,romWord,&a4,willowArcadeGameplayServiceResume,&services};
  uint32_t pc=0x92d2,sp=0xffff00;
  unsigned mainRegisters[15]={0};mainRegisters[13]=0xff8000;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  for(unsigned pass=0;pass<512;pass++){
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n))ram[n^1]=expected[n];
   /* Intercepted main sleep must retain the register set that real 1750 saves. */
   for(unsigned i=0;i<15;i++)m68k_set_reg(i,mainRegisters[i]);
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,sp);
   m68k_set_reg(12,a4);m68k_set_reg(4,d4);m68k_set_reg(16,pc);
   memcpy(nativeInput,expected,65536);
   WillowTransitionStep result=willowNativeGameplayFlowStep(&binding);
   if(!result.supported||result.finished||result.delay!=1)return 1;
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Timeout manager mismatch sample %u pass %u at %x got %x want %x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4)return 1;
   for(unsigned i=0;i<15;i++)mainRegisters[i]=m68k_get_reg(i);
   TARGET_CHECK(0,flow.task.pc==0x932c?1:2);
   checks++;
   if(flow.task.pc==0x92d2){
    sp=m68k_get_reg(15);
    if(sp!=0xfffef8||(m68k_get_reg(0)&65535)!=1)return 1;
    pc=((uint32_t)ram[(sp&65535)^1]<<24)|((uint32_t)ram[((sp+1)&65535)^1]<<16)|((uint32_t)ram[((sp+2)&65535)^1]<<8)|ram[((sp+3)&65535)^1];sp+=4;
    if(pc!=eventWait)return 1;
    waits++;
   }else{
    if(m68k_get_reg(15)!=0xfffefc||eventWait||flow.task.pc!=0x932c||flow.manager.target)return 1;
    if(ram[0xfefe^1]!=0x93||ram[0xfeff^1]!=0x2c||(m68k_get_reg(0)&65535)!=1)return 1;
    if(!rw(0x8008)){
    if(rw(0x020c)!=8||rb(0xf890)||rb(0xf891)||rw(0x801a)!=((rw(0x801c)+4)&252))return 1;
    if(rw(0x8072+rw(0x801c))!=0x0c02||rw(0x8074+rw(0x801c))!=1||flow.initialD1Known||rw(0x8000)!=2)return 1;
    }else{
     if(rw(0x8008)!=2||rw(0x800a)!=3||flow.scene.pc||flow.scene.target)return 1;
     completions++;break;
    }
    pc=0x932c;sp=0xffff00;
   }
   memcpy(nativeInput,expected,65536);
   uint16_t cursor=0,remaining=15;
   for(;;){
    WillowNativeTaskRun run=willowNativeTaskRunOneWithVideo(&runtime,&bus,cursor,remaining,romWord,colorRead,colorWrite,0,0,videoWord);
    if(run.unsupported){fprintf(stderr,"Unsupported scheduled slot %x PC %x\n",run.cursor,runtime.resume[run.cursor/32]);return 1;}
    if(!run.dispatched||run.remaining==65535)break;
    cursor=run.cursor;remaining=run.remaining;
   }
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);
   m68k_set_reg(8,0xff0000);m68k_set_reg(0,15);m68k_set_reg(16,0x14d8);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Scheduler mismatch region %u sample %u pass %u at %x got %x want %x\n",region,sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   TARGET_CHECK(1,1);
   memcpy(nativeInput,expected,65536);
   willowArcadeTaskWake(&bus);
   putRamWord(ram,0xf000,0x10);putRamWord(ram,0xf002,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xfff000);m68k_set_reg(13,0xff8000);m68k_set_reg(16,0x1590);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if(!cpuContext(n)&&ram[n^1]!=expected[n])return 1;
   TARGET_CHECK(2,1);
   if(pass==511)return 1;
  }
 }
 if(completions!=20||!waits||!paletteWrites||videoWrites!=40*videoSize/2+20*(region==1?1024:region==2?256:0))return 1;
 printf("Region %u: %u palette writes and %u selected-region video writes\n",region,paletteWrites,videoWrites);
 printf("Scheduled reset through first scene setup: %u original boundaries, %u actual command sleeps, %u fade-wait completions; semantic RAM, selected video region, palettes and manager D4/A4 match with actual command consumption, fade/clear tasks and wakeups\n",checks,waits,completions);
 printf("Compiled 68000: %u manager/scheduler/wake boundaries match host C and original execution; continuations retained across calls\n",targetChecks);
 return 0;
}
