/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "task_runtime_probe.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifdef WILLOW_TEST_ROM_OWNER
#ifdef WILLOW_TEST_SPLIT_PALETTE
#define PALETTE_SLOTS 2
static unsigned paletteSlotAddress(unsigned n){return n?0x2900:0x2800;}
#else
#define PALETTE_SLOTS 8
static unsigned paletteSlotAddress(unsigned n){return 0x2800+n*34;}
#endif
#endif
static uint8_t rom[0x200000];
#ifdef WILLOW_PROFILE_TASK_PC
static unsigned long long instructionCycles[0x100000];
#endif
static uint16_t probeSource(uint32_t address){return willowMotionWord(rom+address);}
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

/* Prefix layout from the local core header build/genplus-m68k.h. Validate
 * PC and all DAR fields through the exported register API before using it. */
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CoreCyclePrefix;
int main(int argc,char**argv){
 if(argc!=2&&(argc!=3||strcmp(argv[2],"--cycles")))return 2;
 int cycleMode=argc==3;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/task_runtime_probe.bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/task_runtime_probe.sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"taskRuntimeProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100008,0x4e71); /* timing calibration NOP */
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/native-task-runtime-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/native-task-runtime-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 FN(void,m68k_run,(unsigned));
 CoreCyclePrefix*cpu=dlsym(core,"m68k");if(!cpu)return 2;
 if(cycleMode){
  m68k_set_reg(17,0x2700);m68k_set_reg(16,0x100008);
  if(cpu->pc!=0x100008)return 2;
  for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
  cpu->cycles=0;cpu->refresh=128*7;m68k_run(1);
  if(cpu->cycles!=28||m68k_get_reg(16)!=0x10000a){fprintf(stderr,"Cycle calibration failed\n");return 2;}
 }
 unsigned maximumCycles=0,maximumInstructions=0,minimumStack=0xff00;
 unsigned maximumSlices=0,checks=0;
 for(unsigned sample=0;sample<16;sample++){
  memset(probeRam,0,sizeof probeRam);memset(&probeRuntime,0,sizeof probeRuntime);
  const WillowVisibilityBus bus={probeWord,probeByte,probePutWord,probePutByte};
  willowArcadeTaskInitFree(&bus,probeSource);
  for(unsigned n=0;n<128;n++)probePutWord(0x8072+n*2,65535);
  probePutWord(0x801a,sample*16);probePutWord(0x801c,sample*16);
  willowArcadeTaskStart(&bus,0xa0,0x1ddc,0,0,1);probePutWord(0x82c0,1);
  for(unsigned bank=0;bank<6;bank++){probePutWord(0x82a8+bank*4,bank<4?65535:0);probePutWord(0x82aa+bank*4,bank<4?65535:255);}
  for(unsigned n=0;n<3072;n++)probePutColor(0x91c000+n*2,sample*4096+
#ifdef WILLOW_TEST_ROM_OWNER
   probeSource(0xe0000+n*2)
#else
   (n&4095)
#endif
  );
#ifdef WILLOW_TEST_ROM_OWNER
  for(unsigned n=0;n<192;n++)willowMotionPutWord(probeRam+0x2600+n*2,n*16);
  memset(probeRam+0x2780,sample|(sample<<4),96);
  for(unsigned n=0;n<PALETTE_SLOTS;n++)willowMotionPutWord(probeRam+paletteSlotAddress(n),0xffff);
#ifdef WILLOW_TEST_MUTABLE_SEED
  for(unsigned slot=0;slot<PALETTE_SLOTS;slot++){
   unsigned group=(slot*17+sample)%192;
   willowMotionPutWord(probeRam+0x2600+group*2,0xfff1+slot);
   willowMotionPutWord(probeRam+paletteSlotAddress(slot),group);
   for(unsigned n=0;n<16;n++){
    uint16_t base=probeSource(0xe0000+group*32+n*2)^0x111;
    willowMotionPutWord(probeRam+paletteSlotAddress(slot)+2+n*2,base);
    probePutColor(0x91c000+group*32+n*2,base+sample*4096);
   }
  }
#endif
#endif
  willowArcadeEventCommandWaitBegin(&bus,0x1eaa,0);
  for(unsigned n=0;n<65536;n++)ram[n^1]=probeRam[n];
#ifdef WILLOW_TEST_SEGMENTED_TASKS
  WillowNativeTaskSegments segments={probeRam+0x2a00,(WillowNativeTaskControl*)(probeRam+0x2c80),{probeRam+0x2d00,probeRam+0x2e00}};
  uint8_t originalFlat[820];memcpy(originalFlat,probeRam+0x2000,sizeof originalFlat);
  for(unsigned a=0;a<65536;a++){
   uint8_t*p=willowNativeTaskResolve((WillowNativeTaskStore*)(probeRam+0x2000),a);
   uint8_t*q=willowNativeTaskSegmentResolve(&segments,a);
   if(!!p!=!!q)return 2;
   if(p)ram[(q-probeRam)^1]=*p;
  }
#endif
  unsigned pass;
  for(pass=0;pass<80&&probeByte(0xf891);pass++){
   taskRuntimeProbe();
   for(unsigned n=0;n<16;n++)willowMotionPutWord(probeRam+0x2400+n*2,probeRuntime.resume[n]);
   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
   unsigned slices=0;
   if(cycleMode){
    if(cpu->pc!=entry)return 2;
    for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
    cpu->cycles=0;cpu->refresh=128*7;
    unsigned instructions=0;
    while(m68k_get_reg(16)!=0x100000&&instructions<200000){
#ifdef WILLOW_PROFILE_TASK_PC
     unsigned pc=m68k_get_reg(16),before=cpu->cycles;
#endif
     m68k_run(cpu->cycles+1);instructions++;
     unsigned sp=m68k_get_reg(15)&65535;if(sp<minimumStack)minimumStack=sp;
#ifdef WILLOW_PROFILE_TASK_PC
     if(pc<0x200000)instructionCycles[pc>>1]+=(unsigned)cpu->cycles-before;
#endif
    }
    if(cpu->cycles<0)return 2;
    if((unsigned)cpu->cycles>maximumCycles)maximumCycles=cpu->cycles;
    if(instructions>maximumInstructions)maximumInstructions=instructions;
   }else do{retro_run();slices++;}while(m68k_get_reg(16)!=0x100000&&slices<40);
   if(m68k_get_reg(16)!=0x100000||willowMotionWord(probeRam+0x1002)!=0x5254){fprintf(stderr,"Target runtime stopped PC %x\n",m68k_get_reg(16));return 1;}
#ifdef WILLOW_TEST_ROM_OWNER
   WillowNativePaletteOwner owner;WillowMutablePaletteGroup slots[PALETTE_SLOTS];
   for(unsigned n=0;n<192;n++)owner.sources.group[n]=((uint16_t)ram[(0x2600+n*2)^1]<<8)|ram[(0x2601+n*2)^1];
   for(unsigned n=0;n<96;n++)owner.view.offsets[n]=ram[(0x2780+n)^1];
   for(unsigned n=0;n<PALETTE_SLOTS;n++){
    slots[n].group=((uint16_t)ram[(paletteSlotAddress(n))^1]<<8)|ram[(paletteSlotAddress(n)+1)^1];
    for(unsigned j=0;j<16;j++)slots[n].words[j]=((uint16_t)ram[(paletteSlotAddress(n)+2+j*2)^1]<<8)|ram[(paletteSlotAddress(n)+3+j*2)^1];
   }
#endif
   for(unsigned n=0;n<3072;n++){
#ifdef WILLOW_TEST_ROM_OWNER
    uint16_t actual;
    if(!willowMutablePaletteRead(&owner,slots,PALETTE_SLOTS,0x91c000+n*2,probeSource,&actual)||actual!=willowMotionWord(probeRam+0x4000+n*2)){
     fprintf(stderr,"ROM owner mismatch sample %u pass %u color %u\n",sample,pass,n);return 1;
    }
#else
    unsigned at=0x4000+n*2,group=n/16;
    uint16_t raw=((uint16_t)ram[at^1]<<8)|ram[(at+1)^1];
    unsigned offset=(ram[(0x2500+group/2)^1]>>((group&1)*4))&15;
    if((uint16_t)(raw+(offset<<12))!=willowMotionWord(probeRam+at)){
     fprintf(stderr,"Lazy target palette mismatch sample %u pass %u color %u\n",sample,pass,n);return 1;
    }
#endif
   }
#ifdef WILLOW_TEST_SEGMENTED_TASKS
   for(unsigned a=0;a<65536;a++){
    uint8_t*p=willowNativeTaskResolve((WillowNativeTaskStore*)(probeRam+0x2000),a);
    uint8_t*q=willowNativeTaskSegmentResolve(&segments,a);
    if(p && *p!=ram[(q-probeRam)^1]){fprintf(stderr,"Segmented task mismatch %04x pass %u\n",a,pass);return 1;}
   }
   for(unsigned n=0;n<820;n++)if(ram[(0x2000+n)^1]!=originalFlat[n])return 1;
#endif
   for(unsigned n=0;n<0xfc00;n++)if(!(n>=0x2500&&n<0x2560)&&!(n>=0x4000&&n<0x5800)
#ifdef WILLOW_TEST_ROM_OWNER
    &&!(n>=0x2600&&n<0x27e0)
#ifdef WILLOW_TEST_SPLIT_PALETTE
    &&!(n>=0x2800&&n<0x2822)&&!(n>=0x2900&&n<0x2922)
#else
    &&!(n>=0x2800&&n<0x2910)
#endif
#endif
#ifdef WILLOW_TEST_SEGMENTED_TASKS
    &&!(n>=0x2000&&n<0x2334)&&!(n>=0x2a00&&n<0x2c00)&&!(n>=0x2c80&&n<0x2cb4)
    &&!(n>=0x2d00&&n<0x2d80)&&!(n>=0x2e00&&n<0x2e80)
#endif
    &&ram[n^1]!=probeRam[n]){
    fprintf(stderr,"Target runtime mismatch sample %u pass %u address %04x got %02x want %02x\n",sample,pass,n,ram[n^1],probeRam[n]);return 1;
   }
   if(slices>maximumSlices)maximumSlices=slices;
   checks++;
  }
  if(pass==80||probeWord(0x020c)!=8||probeWord(0x82c0))return 1;
 }
#ifdef WILLOW_PROFILE_TASK_PC
 if(cycleMode)for(unsigned rank=0;rank<24;rank++){
  unsigned best=0;for(unsigned at=0;at<0x100000;at++)if(instructionCycles[at]>instructionCycles[best])best=at;
  printf("Hot instruction %06x: %llu aggregate master cycles\n",best*2,instructionCycles[best]);instructionCycles[best]=0;
 }
#endif
 if(cycleMode)printf("Isolated CPU timing: worst %u master cycles (%.1f 68000 cycles), maximum %u instruction steps; bus refresh included, VDP/DMA/contention excluded\n",maximumCycles,maximumCycles/7.0,maximumInstructions);
 if(!cycleMode)printf("Native 68000 continuation runtime: 16 reveal sequences, %u scan/wake passes; compact store, continuations, globals and palettes match; worst pass %u emulated slices, no renderer/VDP workload\n",checks,maximumSlices);
 if(cycleMode)printf("Cycle-mode semantic comparisons: %u passes match; maximum observed stack descent %u bytes below entry SP\n",checks,0xff00-minimumStack);
 return 0;
}
