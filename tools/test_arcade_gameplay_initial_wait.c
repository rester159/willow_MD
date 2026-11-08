/* Original main-task initial 1ECC wait through first ordinary loop sleep.
 * Only 1750 sleep is intercepted; completion is a controlled external input. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_native_gameplay_flow.h"
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
static uint16_t sourceWord(uint32_t a){return willowMotionWord(rom+a);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/gameplay-initial-wait-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/gameplay-initial-wait-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,waits=0,completions=0;
 for(unsigned sample=0;sample<512;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31);
  ww(0x8000,2);ww(0x8004,6);ww(0x8008,0);
  ww(0x801a,(sample*4)&252);ww(0x801e,(sample*2)&126);
  wb(0x828f,(sample&64)?1:0);wb(0x8068,2);wb(0x8058,0);
  wb(0x839e,sample);ww(0x839c,(sample&128)?65535:0);
  wb(0x8342,sample);wb(0x8346,sample);wb(0xf891,0);
  uint16_t d4=sample*977,eventWait=0;uint32_t a4=0x123456;
  WillowNativeGameplayFlow flow={0};
  WillowArcadeGameplayServices services={.bus=&bus,.d4=&d4,.a4=&a4,.eventWait=&eventWait};
  /* Life-loss table read is the only ROM data access after the wait. */
  services.busWord=sourceWord;
  WillowNativeGameplayFlowBinding binding={&flow,&bus,sourceWord,&a4,willowArcadeGameplayServiceResume,&services};
  uint32_t pc=0x92d2,sp=0xffff00;unsigned sleeps=0;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  for(unsigned pass=0;pass<8;pass++){
   if(sleeps>=1+sample%4)wb(0xf891,0);
   for(unsigned n=0;n<0xfe80;n++)ram[n^1]=expected[n];
   for(unsigned n=0xff04;n<65536;n++)ram[n^1]=expected[n];
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,sp);
   m68k_set_reg(12,a4);m68k_set_reg(4,d4);m68k_set_reg(16,pc);
   WillowTransitionStep result=willowNativeGameplayFlowStep(&binding);
   if(!result.supported||result.finished||result.delay!=1)return 1;
   retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
   for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Initial wait mismatch sample %u pass %u at %x got %x want %x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4)return 1;
   checks++;sp=m68k_get_reg(15);
   if((m68k_get_reg(0)&65535)!=1)return 1;
   pc=((uint32_t)ram[(sp&65535)^1]<<24)|((uint32_t)ram[((sp+1)&65535)^1]<<16)|((uint32_t)ram[((sp+2)&65535)^1]<<8)|ram[((sp+3)&65535)^1];
   if(flow.task.pc==0x92d2){
    if(sp!=0xfffef8||pc!=0x1ea2||eventWait!=pc)return 1;
    waits++;sleeps++;sp+=4;
   }else{
    if(sp!=0xfffefc||pc!=0x932c||flow.task.pc!=pc||eventWait||flow.manager.target)return 1;
    completions++;break;
   }
   if(pass==7)return 1;
  }
 }
 if(completions!=512||waits!=1280)return 1;
 printf("Initial gameplay wait: %u original boundaries, %u mandatory/repeated command sleeps, %u transitions into ordinary main-loop sleep; actual producer, life-loss/paused paths, full non-stack RAM and D4/A4 match; completion externally controlled\n",checks,waits,completions);
 return 0;
}
