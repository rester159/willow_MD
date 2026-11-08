/* Differential verification of boot state handlers against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_attract_task.h"
#include "../src/willow_arcade_boot_options.h"
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
static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}

static void colorWrite(uint32_t at,uint16_t v){ww(0x2000+at-0x91c000,v);}
static void videoWord(uint32_t at,uint16_t v){if(at>=0x910000&&at<0x914000)ww(0x9000+at-0x910000,v);}

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 /* Execute all boot handlers and nested calls; stop before scheduler services.
  * Resume by returning from the intercepted sleep with live registers intact. */
 willowMotionPutWord(rom+0x1750,0x60fe);
 willowMotionPutWord(rom+0x1636,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 const unsigned patches[]={0xae4,0xb06,0xaf6,0xb16,0x988,0x99a,0x9b0,0x9cc,0x9da};
 for(unsigned n=0;n<9;n++){uint32_t at=willowMotionLong(rom+patches[n]);willowMotionPutLong(rom+patches[n],at-0x91c000+0xff2000);}
 willowMotionPutLong(rom+0x65a6,0xff9000);
 willowMotionPutLong(rom+0x88,0x1690);willowMotionPutLong(rom+0x9c,0x160e);willowMotionPutLong(rom+0xac,0x170c);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/attract-task-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/attract-task-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};


 unsigned checks=0,finished=0;
 for(unsigned mode=0;mode<8;mode++)for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadeTaskInitFree(&bus,romWord);
  for(unsigned n=0;n<16;n++)wb(n*32,0);
  ww(0x8000,mode==3?0:mode==7?4:(uint16_t)(2+4*(sample%3)));ww(0x8004,mode==4?0:2);ww(0x82c0,mode==6?1:0);
  wb(0x8058,sample&0x30);wb(0x8059,(sample&1)?sample&0x30:0);
  wb(0x82a4,sample&2);wb(0x82a2,(sample&4)?9:0);
  wb(0x828f,mode==7);wb(0x8068,2);wb(0xf891,mode==1);
  ww(0x801a,(sample%64)*4);
  WillowNativeAttractTask task={mode==0?0x80d0:mode<3?0x1ea2:0x810e,(uint16_t)(sample*311),(uint16_t)(sample*977)};
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);
  m68k_set_reg(1,task.d1);m68k_set_reg(2,task.d2);
  /* A resumed wait returns to 80DA; source 1EA2 needs that caller frame. */
  if(task.pc==0x1ea2){ram[0xf000^1]=0;ram[0xf001^1]=0;ram[0xf002^1]=0x80;ram[0xf003^1]=0xda;}
  m68k_set_reg(16,task.pc);
  WillowTransitionStep step=willowNativeAttractTaskStep(&task,&bus,romWord,colorWrite,videoWord);
  if(!step.supported)return 1;
  unsigned stop=step.finished?0x1636:0x1750;
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Attract task stop mode %u sample %u PC %x\n",mode,sample,m68k_get_reg(16));return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xef80&&n<0xf008)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Attract task RAM mode %u sample %u at %x actual %x expected %x\n",mode,sample,n,ram[n^1],expected[n]);return 1;
  }
  if(!step.finished){
   unsigned sp=m68k_get_reg(15)&65535;
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=step.resume||(m68k_get_reg(0)&65535)!=step.delay||(m68k_get_reg(1)&65535)!=task.d1||(m68k_get_reg(2)&65535)!=task.d2){fprintf(stderr,"Attract task continuation mode %u sample %u D1 %x/%x D2 %x/%x\n",mode,sample,m68k_get_reg(1),task.d1,m68k_get_reg(2),task.d2);return 1;}
  }
  checks++;finished+=step.finished;
 }
 /* Unsupported bodies/callbacks must not partially advance a task. */
 for(unsigned guard=0;guard<5;guard++){
  WillowNativeAttractTask task={0x810e,123,456},before=task;
  wb(0x8058,guard==0?0x40:0);wb(0x828f,0);
  ww(0x8000,guard==1?4:2);ww(0x8004,guard==2?4:2);ww(0x82c0,1);
  uint8_t snapshot[65536];memcpy(snapshot,expected,sizeof snapshot);
  WillowTransitionStep step=willowNativeAttractTaskStep(&task,&bus,guard==3?NULL:romWord,colorWrite,guard==4?NULL:videoWord);
  if(step.supported||memcmp(&task,&before,sizeof task)||memcmp(expected,snapshot,sizeof snapshot))return 1;
 }
 printf("Attract task: %u source sleep/finish boundaries, %u handoffs; initial wait, known managers, input transitions and live parameters match\n",checks,finished);
 return 0;
}
