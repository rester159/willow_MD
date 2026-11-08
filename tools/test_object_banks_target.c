/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_object_banks.h"
static WillowNativeObjectBanks probeStore;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
#define OBJECT_PROBE_FILE "build/object_banks_cartridge_probe"
#else
#define OBJECT_PROBE_FILE "build/object_banks_probe"
#endif
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

typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} ObjectCoreCycles;
int main(int argc,char**argv){
 if(argc!=2&&(argc!=3||strcmp(argv[2],"--cycles")))return 2;
 unsigned cycleMode=argc==3;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen(OBJECT_PROBE_FILE ".bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen(OBJECT_PROBE_FILE ".sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"objectBanksProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
#endif
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100004,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/object-banks-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/object-banks-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;


 FN(void,m68k_run,(unsigned));ObjectCoreCycles*cpu=dlsym(core,"m68k");
 unsigned maximumCycles[5]={0},maximumSlices=0,checks=0;
 if(cycleMode){
  if(!cpu)return 2;
  m68k_set_reg(16,0x100004);if(cpu->pc!=0x100004)return 2;
  for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
  cpu->cycles=0;cpu->refresh=896;m68k_run(1);
  if(cpu->cycles!=28||m68k_get_reg(16)!=0x100006)return 2;
 }
 uint8_t expected[0xfe00];
 uint8_t*storeMemory=ram,*expectedMemory=expected;unsigned byteXor=1;
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
 FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 storeMemory=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!storeMemory||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)!=65536)return 2;
 uint8_t expectedSram[65536];expectedMemory=expectedSram;byteXor=0;
 memset(storeMemory,0xa5,65536);
#endif
 memset(ram,0xa5,65536);
 ram[0x1010^1]=0x90;ram[0x1011^1]=0;
 WillowNativeObjectPort port={0x9000};
 for(unsigned n=0;n<1024;n++){
  probeStore.bank[0][n]=n*73+3;probeStore.bank[1][n]=n*31+7;probeStore.displayed[n]=n^0x5a5a;
 }
 for(unsigned n=0;n<sizeof probeStore/2;n++){
  uint16_t value=((uint16_t*)&probeStore)[n];
  storeMemory[(0x2002+n*2)^byteXor]=value>>8;storeMemory[(0x2003+n*2)^byteXor]=value;
 }
 for(unsigned sample=0;sample<768;sample++){
  unsigned op=0,at=0x900000,value=sample*71,second=sample^0x1234,count=512,selector=0x9000;
  switch(sample%16){
   case 0:break;
   case 1:at=0x9087fe;break;
   case 2:op=2;break;
   case 3:op=1;value=second=0;break;
   case 4:op=2;selector=0x9080;break;
   case 5:op=1;at=0x908000;value=second=0;break;
   case 6:at=0x900000+(sample*23%1024)*2;break;
   case 7:op=1;at=0x9087fe;count=1;break;
   case 8:at=0x900001;break;
   case 9:op=2;selector=0x91c0;break;
   case 10:op=1;at=0x908000+(sample%128)*2;count=128;break;
   case 11:op=2;selector=0x9007;break;
   case 12:op=3;at=0x100;value=0x9080;break;
   case 13:op=4;break;
   case 14:op=3;at=0x102;value=0x9000;break;
   case 15:op=3;at=0x100;value=0x9000;break;
  }
  const uint16_t params[]={op,0x7777,at>>16,at,value,second,count,selector};
  for(unsigned n=0;n<8;n++){ram[(0x1000+n*2)^1]=params[n]>>8;ram[(0x1001+n*2)^1]=params[n];}
  memcpy(expected,ram,sizeof expected);
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
  memcpy(expectedSram,storeMemory,sizeof expectedSram);
#endif
  unsigned ok=op==0?willowObjectWrite(&probeStore,at,value):
    op==1?willowObjectFill(&probeStore,at,value,second,count):
    op==2?willowObjectLatch(&probeStore,selector):
    op==3?willowObjectPortWrite(&port,at,value):willowObjectPortLatch(&probeStore,&port);
  expected[0x1010^1]=port.selector>>8;expected[0x1011^1]=port.selector;
  unsigned status=ok?0x544b:1;
  expected[0x1002^1]=status>>8;expected[0x1003^1]=status;
  for(unsigned n=0;n<sizeof probeStore/2;n++){
   uint16_t v=((uint16_t*)&probeStore)[n];
   expectedMemory[(0x2002+n*2)^byteXor]=v>>8;expectedMemory[(0x2003+n*2)^byteXor]=v;
  }
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  for(unsigned r=2;r<=14;r++)if(r!=8&&r!=9)m68k_set_reg(r,0x12340000+sample*31+r*997);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  unsigned slices=0;
  if(cycleMode){
   cpu->cycles=0;cpu->refresh=896;unsigned instructions=0;
   while(m68k_get_reg(16)!=0x100000&&instructions++<100000)m68k_run(cpu->cycles+1);
   if(cpu->cycles<0)return 2;
   if((unsigned)cpu->cycles>maximumCycles[op])maximumCycles[op]=cpu->cycles;
  }else do{retro_run();slices++;}while(m68k_get_reg(16)!=0x100000&&slices<20);
  for(unsigned r=2;r<=14;r++)if(r!=8&&r!=9&&m68k_get_reg(r)!=0x12340000+sample*31+r*997){
   fprintf(stderr,"Object bank ABI register %u changed in sample %u\n",r,sample);return 1;
  }
  if(m68k_get_reg(16)!=0x100000||memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Target object banks mismatch sample %u op %u PC %x\n",sample,op,m68k_get_reg(16));return 1;
  }
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
  if(memcmp(expectedSram,storeMemory,sizeof expectedSram)){
   fprintf(stderr,"Cartridge object banks mismatch sample %u\n",sample);return 1;
  }
#endif
  if(slices>maximumSlices)maximumSlices=slices;checks++;
 }
#ifdef WILLOW_OBJECT_PROBE_CARTRIDGE
 puts("Cartridge RAM: declared 64 KiB 16-bit mapping, complete SRAM guards and main RAM preservation pass");
#endif
 if(cycleMode)printf("Isolated maximum master cycles: word write %u, pair fill %u, bank latch %u; calibrated 4-cycle NOP=28 master cycles; excludes game/VDP/audio costs\n",maximumCycles[0],maximumCycles[1],maximumCycles[2]);
 printf("68000 object banks: %u sequential writes/fills/latches, all 6144 bytes, callee-saved registers and unrelated non-stack RAM match; maximum %u frame slices (zero in instruction-step mode)\n",checks,maximumSlices);
 return 0;
}
