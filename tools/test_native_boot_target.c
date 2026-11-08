/* Execute compiled 68000 boot adapter against the source-verified host path. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_task_scheduler.h"
#include "../src/willow_arcade_tasks.h"
#include "../src/willow_native_boot_runtime.h"
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

static uint8_t expected[65536];
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint16_t source(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t color(uint32_t a){return rw(0x2000+a-0x91c000);}
static void putColor(uint32_t a,uint16_t v){ww(0x2000+a-0x91c000,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);

 f=fopen("build/boot_runtime_probe.bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/boot_runtime_probe.sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"bootRuntimeProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/boot-target-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/boot-target-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const WillowVisibilityBus bus={rw,rb,ww,wb};

 unsigned checks=0,finishes=0,maximumSlices=0;
 for(unsigned state=0;state<6;state++)for(unsigned sample=0;sample<64;sample++){
  memset(expected,0,sizeof expected);
  WillowNativeTaskRuntime runtime={0};WillowNativeBootTask boot={0};
  willowArcadeTaskInitFree(&bus,source);
  for(unsigned n=0;n<16;n++){ww(n*32+12,0xff);ww(n*32+14,0x4100+n*256);}
  ww(0x8000,state*2);ww(0x8002,sample%3);ww(0x801a,(sample&63)*4);
  wb(0x8066,sample);wb(0x8067,sample*73);wb(0x8068,sample*151);
  wb(0x8058,(sample&1)?0x40:0);wb(0x828f,(sample&2)?128:0);
  wb(0x20,(sample&4)?0:12);wb(0x80,(sample&8)?0:12);
  wb(0x82a2,sample);wb(0x82a6,1);
  willowArcadeTaskStart(&bus,0,0x7fda,sample*977,sample*317,1);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  for(unsigned pass=0;pass<24;pass++){
   if(pass%4==3){wb(0xf891,0);ram[0xf891^1]=0;wb(0x82a6,0);ram[0x82a6^1]=0;}
   WillowNativeTaskRun run=willowNativeTaskRunWithBoot(&runtime,&boot,&bus,0,0,source,color,putColor,0,0);
   if(run.unsupported||!run.dispatched){fprintf(stderr,"Boot dispatch failed state %u sample %u pass %u\n",state,sample,pass);return 1;}


   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
   unsigned slices=0;
   do{retro_run();slices++;}while(m68k_get_reg(16)!=0x100000&&slices<32);
   if(slices>maximumSlices)maximumSlices=slices;
   if(m68k_get_reg(16)!=0x100000||ram[0x1002^1]!=0x42||ram[0x1003^1]!=0x4f)return 1;
   for(unsigned n=0;n<65536;n++)if(n!=0x1002&&n!=0x1003&&!(n>=2&&n<12)&&!(n>=0x3000&&n<0x3020)&&n<0xfc00&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Target boot mismatch state %u sample %u pass %u address %x got %x expected %x\n",state,sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   for(unsigned n=0;n<16;n++)if((((unsigned)ram[(0x3000+n*2)^1]<<8)|ram[(0x3001+n*2)^1])!=runtime.resume[n])return 1;
   const uint16_t fields[]={boot.waitReturn,boot.d1,boot.d2},offsets[]={2,4,8};
   for(unsigned n=0;n<3;n++)if((((unsigned)ram[offsets[n]^1]<<8)|ram[(offsets[n]+1)^1])!=fields[n])return 1;
   checks++;
   if(!rb(0)){finishes++;if(rw(0x020c)!=8||runtime.resume[0]||boot.pc)return 1;break;}

   willowArcadeTaskWake(&bus);
   /* Mirror the separately verified source wake operation as input. */
   for(unsigned n=0;n<16;n++){ram[(n*32)^1]=expected[n*32];ram[(n*32+1)^1]=expected[n*32+1];}
  }
 }

 printf("Compiled boot adapter: %u dispatches, %u terminations; logical RAM, 32-byte runtime and boot words retained in slot-zero CPU context match; maximum %u emulated slices (not full-frame timing)\n",checks,finishes,maximumSlices);
 return 0;
}
