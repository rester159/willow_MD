/* Original shop input/animation and return-to-game sprite clear, with real services. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_arcade_shop_resume.h"
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
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static void videoWord(uint32_t at,uint16_t v){
 if(at>=0x910000&&at<0x914000)ww(0x4000+at-0x910000,v);
 else if(at>=0x900000&&at<0x900800)ww(0x9000+at-0x900000,v);
 else if(at>=0x908000&&at<0x908800)ww(0xa000+at-0x908000,v);
 else if(at>=0x917aa8&&at<0x917ad0)ww(0x9800+at-0x917aa8,v);
 else abort();
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutLong(rom+0xa8,0x15f2);
 const unsigned operands[]={0x7d84,0x34a00,0x34b0a,0x6620,0x662a};
 const unsigned original[]={0x910200,0x900000,0x917aa8,0x900000,0x908000};
 const unsigned mapped[]={0xff4200,0xff9000,0xff9800,0xff9000,0xffa000};
 for(unsigned n=0;n<5;n++){if(willowMotionLong(rom+operands[n])!=original[n])return 2;willowMotionPutLong(rom+operands[n],mapped[n]);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/shop-input-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/shop-input-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0,rejections=0;
 for(unsigned mode=0;mode<3;mode++)for(unsigned sample=0;sample<(mode==2?12544:mode?64:2048);sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x800c,2);ww(0x8010,0);ww(0x0f10,(sample>>10)*2);
  wb(0x806a,sample);wb(0x806b,((sample>>8)&3)*85);
  wb(0x0f38,sample%64);wb(0x0f39,sample&1);wb(0x0f3a,sample%11);wb(0x0f3b,0);
  ww(0x8344,(sample/256)%7);ww(0x8374,(sample/1792)%7);
  ww(0x801e,(sample%64)*2);wb(0x8290,(sample/64)&1);wb(0x8293,(sample/128)&1);
  uint16_t d4=sample*977;uint32_t a4=0x123456;
  WillowArcadeGameplayServices services={.bus=&bus,.busWord=word,.d4=&d4,.a4=&a4,.videoWrite=videoWord,.textWrite=videoWord,.spriteWord=videoWord};
  WillowArcadeShopTask task={0};
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint8_t pressed=(uint8_t)(rb(0x806a)&~rb(0x806b));
  uint8_t result=mode?willowArcadeGameplayServiceResume(&services,mode==2?0x7878:0x6614,-1):willowArcadeShopResume(&task,&bus,word,willowArcadeGameplayServiceResume,&services);
  if(!mode&&!(pressed&3)&&(pressed&0x30)&&!(sample>>10)){
   if(result)return 1;
   for(unsigned n=0;n<65536;n++)if(expected[n]!=ram[n^1])return 1;
   rejections++;continue;
  }
  if(result!=1||task.pc)return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(4,sample*977);m68k_set_reg(12,0x123456);m68k_set_reg(16,mode==2?0x7878:mode?0x6614:0x33bd6);
  for(unsigned slice=0;slice<16&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Shop control mode %u sample %u RAM %x got %x want %x\n",mode,sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4)return 1;
  checks++;
 }
 printf("Shop controls/sprite clear/music: %u original-CPU cases, %u unsupported purchases rejected before RAM mutation; text, cursor, animation both sprite banks and stage/section sound queues match\n",checks,rejections);
 return 0;
}
