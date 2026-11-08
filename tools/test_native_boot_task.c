/* Differential verification of boot state handlers against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_boot_task.h"
#include "../src/willow_arcade_boot_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "demo_state_lifetime_audit.h"
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
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/boot-task-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/boot-task-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};


 unsigned checks=0,finished=0;
 const uint16_t counts[]={0,1,2,255,256,65535};
 for(unsigned start=0;start<7;start++)for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x8000,start?2*(start-1):0);ww(0x8002,counts[sample%6]);
  ww(0x801a,(sample&63)*4);wb(0x82a6,1);
  wb(0x8066,sample);wb(0x8067,sample*73);wb(0x8068,sample*151);
  wb(0x8058,(sample&1)?0x40:0);wb(0x828f,(sample&2)?128:0);
  wb(0x20,(sample&4)?0:12);wb(0x80,(sample&8)?0:12);
  wb(0x82a2,sample);
  uint8_t demoBefore[321];demoLifetimeCapture(demoBefore,expected);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowNativeBootTask task={start?0x7fde:0x7fda,0,(uint16_t)(sample*977),(uint16_t)(sample*317)};
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);
  m68k_set_reg(1,task.d1);m68k_set_reg(2,task.d2);m68k_set_reg(16,task.pc);
  for(unsigned pass=0;pass<24;pass++){
   /* Controlled external command completion, not a simulation of its body. */
   if(pass%4==3){wb(0xf891,0);ram[0xf891^1]=0;wb(0x82a6,0);ram[0x82a6^1]=0;}
   WillowTransitionStep step=willowNativeBootTaskStep(&task,&bus,romWord);
   if(!step.supported)return 1;
   retro_run();
   unsigned stop=m68k_get_reg(16);
   if(stop!=(step.finished?0x1636:0x1750)){fprintf(stderr,"Boot task stop start %u sample %u pass %u PC %x finished %u\n",start,sample,pass,stop,step.finished);return 1;}
   if((m68k_get_reg(1)&65535)!=task.d1||(m68k_get_reg(2)&65535)!=task.d2){fprintf(stderr,"Boot task live parameter mismatch\n");return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xef80&&n<0xf004)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Boot task RAM start %u sample %u pass %u address %x\n",start,sample,pass,n);return 1;
   }
   if(!demoLifetimePreserved(demoBefore,ram))return 1;
   checks++;
   if(step.finished){finished++;break;}
   if((m68k_get_reg(0)&65535)!=step.delay)return 1;
   unsigned sp=m68k_get_reg(15)&65535;
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=step.resume){fprintf(stderr,"Boot task continuation %x expected %x\n",resume,step.resume);return 1;}
   m68k_set_reg(15,m68k_get_reg(15)+4);m68k_set_reg(16,resume);
  }
 }
 printf("Native boot task: %u source sleep/finish boundaries, %u handoffs; nested waits, mandatory sleeps, live D1/D2, queue state and all non-stack RAM match\n",checks,finished);
 puts("Supplemental demo lifetime: all 321 source bytes preserved at every tested CPU boundary; external service bodies excluded where the fixture suspends");
 return 0;
}
