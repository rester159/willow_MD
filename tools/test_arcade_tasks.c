/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_tasks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
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
static uint16_t sourceWord(uint32_t at){return willowMotionWord(rom+at);}
int main(int argc,char**argv){
 if(argc!=2 && (argc!=3 || strcmp(argv[2],"--native-cancel-fixtures")))return 2;
 FILE*fixtures=argc==3?fopen("build/player-cancel-original.bin","wb"):NULL;
 if(argc==3&&!fixtures)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Exercise actual trap wrappers and handlers, including exception
  * entry/RTE. Only the independent VBlank scan's context-restore epilogue
  * is replaced by RTS, because this fixture has no interrupt prologue. */
 willowMotionPutLong(rom+0x80,0x15d0); /* trap 0 */
 willowMotionPutLong(rom+0xa8,0x15f2); /* trap 10 */
 willowMotionPutLong(rom+0x88,0x1690); /* trap 2 */
 willowMotionPutLong(rom+0x9c,0x160e); /* trap 7 */
 willowMotionPutWord(rom+0x15b4,0x4e75);
 willowMotionPutLong(rom+0x94,0x17b0); /* trap 5 */
 willowMotionPutLong(rom+0x98,0x17cc); /* trap 6 */
 /* Stop initialization before unrelated startup; completion tests enter
  * after trap-frame discard and stop before scheduler context restoration. */
 willowMotionPutWord(rom+0x1466,0x4e75);
 willowMotionPutWord(rom+0x14ac,0x4e75);
 willowMotionPutWord(rom+0x151e,0x4e75);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/task-services-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/task-services-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 const uint32_t entries[]={0x15cc,0x15ee,0x168c,0x1590,0x160a,0x144e,0x1490,0x163a,0x165e,0x17a4,0x17c0};
 unsigned checks=0;
 for(unsigned e=0;e<11;e++)for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  uint16_t slot=(sample&15)*32;
  uint32_t entry=0x12345678UL+sample*77;
  uint16_t d0=e==4?sample*53:slot,d1=sample*977,d2=sample*317;
  const uint8_t states[]={0,1,4,8,12,255,2,128};
  for(unsigned n=0;n<16;n++){
   wb(n*32,states[((sample>>4)+n)%8]);wb(n*32+1,(sample>>4)+n*73);
  }
  /* Eight real dynamic slots and a valid original-address pointer stack. */
  ww(0x0208,0x00ff);ww(0x020a,0x3000+(sample%9)*4);
  ww(0x020c,8-sample%9);
  for(unsigned n=0;n<8;n++){ww(0x3000+n*4,0x00ff);ww(0x3002+n*4,0x100+n*32);}
  ww(0x0204,0x00ff);ww(0x0206,slot);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint8_t fixture[66];
  if(fixtures&&e==2){willowMotionPutWord(fixture,slot);memcpy(fixture+2,expected+slot,32);}
  switch(e){
  case 0:willowArcadeTaskStart(&bus,slot,entry,d1,d2,0);break;
  case 1:willowArcadeTaskStart(&bus,slot,entry,d1,d2,1);break;
  case 2:willowArcadeTaskKill(&bus,slot);break;
  case 3:willowArcadeTaskWake(&bus);break;
  case 4:if(willowArcadeTaskAllocate(&bus,entry,d0,d1)!=(sample%9!=8))return 1;break;
  case 5:willowArcadeTaskInitStacks(&bus,sourceWord);break;
  case 6:willowArcadeTaskInitFree(&bus,sourceWord);break;
  case 7:willowArcadeTaskFinish(&bus,0);break;
  case 8:willowArcadeTaskFinish(&bus,1);break;
  case 9:willowArcadeTaskSuspend(&bus,slot);break;
  case 10:willowArcadeTaskResume(&bus,slot,d1);break;
  }
  m68k_set_reg(8,entry);m68k_set_reg(1,d1);m68k_set_reg(2,d2);
  RUN(entries[e],d0);
  for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
   if(ram[n^1]!=expected[n]){fprintf(stderr,"Task service mismatch entry %06x sample %u address %04x got %02x want %02x\n",entries[e],sample,n,ram[n^1],expected[n]);return 1;}
  }
  if(fixtures&&e==2){
   for(unsigned n=0;n<32;n++)fixture[34+n]=ram[(slot+n)^1];
   if(fwrite(fixture,1,sizeof fixture,fixtures)!=sizeof fixture)return 2;
  }
  checks++;
 }
 if(fixtures&&fclose(fixtures))return 2;
 unsigned sequenceChecks=0;
 for(unsigned sequence=0;sequence<64;sequence++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sequence*37+n*3)^0x6b;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint16_t allocated[8];
  for(unsigned step=0;step<27;step++){
   uint32_t pc;
   uint32_t entry=0x78f0;
   uint16_t d0=sequence*7+step,d1=sequence*17+step;
   if(step==0){willowArcadeTaskInitStacks(&bus,sourceWord);pc=0x144e;}
   else if(step==1){willowArcadeTaskInitFree(&bus,sourceWord);pc=0x1490;}
   else if(step<11||step>=19){
    if(step<10)allocated[step-2]=rw(rw(0x020a)+2);
    uint8_t result=willowArcadeTaskAllocate(&bus,entry,d0,d1);
    if(result!=(step!=10))return 1;
    pc=0x160a;
   }else{
    /* Release a permutation, then verify the resulting LIFO reallocation.
     * Set only the scheduler's current-record input in both executions. */
    uint16_t slot=allocated[((step-11)*3+sequence)%8];
    ww(0x0204,0x00ff);ww(0x0206,slot);
    putRamWord(ram,0x0204,0x00ff);putRamWord(ram,0x0206,slot);
    willowArcadeTaskFinish(&bus,1);pc=0x165e;
   }
   m68k_set_reg(8,entry);m68k_set_reg(1,d1);
   RUN(pc,d0);
   for(unsigned n=0;n<65536;n++)if(n<0xfe80||n>=0xff04){
    if(ram[n^1]!=expected[n]){fprintf(stderr,"Task lifecycle mismatch sequence %u step %u address %04x\n",sequence,step,n);return 1;}
   }
   sequenceChecks++;
  }
 }
 printf("Source task lifecycle: %u consecutive operations covering initialization, exhaustion, permuted release and LIFO reuse; RAM retained between operations\n",sequenceChecks);
 printf("Source task records: %u start/replacement/kill/wake/allocation/initialization/completion/suspend/resume cases; original service instructions, all non-stack RAM match\n",checks);
 return 0;
}
