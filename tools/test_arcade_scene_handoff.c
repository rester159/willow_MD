/* Actual intro handoff and its shared services compared with original execution. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_native_gameplay_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],source[0x100000];
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



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(source+at);}
static uint8_t*resolve(uint16_t at){return expected+at;}
static uint16_t busRomWord(uint32_t at){return sourceWord(at);}
static void textWrite(uint32_t at,uint16_t v){ww(0x4000+at-0x910000,v);}

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);memcpy(source,rom,sizeof source);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const unsigned patches[]={0x6644,0x7d84,0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910000,0x910200,0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scene-handoff-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scene-handoff-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 const uint32_t entries[]={0x7d80,0x6642,0x4f44,0x4035e,0x9a92,0x93be};
 for(unsigned mode=0;mode<6;mode++)for(unsigned sample=0;sample<512;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint32_t pc=entries[mode];willowArcadePoolsInit(&bus);
  if(sample&1){
   const uint16_t counts[]={0x9801,0x9804,0x980a,0x980d,0x9810,0x9813,0xf48c};
   for(unsigned n=0;n<sizeof counts/sizeof counts[0];n++)wb(counts[n],0);
  }
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample%17?1:0);
  ww(0x8344,mode==4?sample%7:sample%5);ww(0x838c,sample*13);
  ww(0x984c,sample*31);ww(0x9850,sample*7);
  /* Exercise object scan exhaustion, including its one-past-pool A4. */
  for(unsigned n=0;n<48;n++)wb(0x8502+n*64,n<sample%49?1:0);
  ww(0x8000,2);ww(0x8004,0);ww(0x8008,24);ww(0x800a,1);
  wb(0x82a6,0);ww(0x8070,2);ww(0x828a,0);ww(0x828c,0x183c);

  wb(0xeae7,sample%12);
  wb(0x8366,(sample/12)%7);wb(0x8367,(sample/84)%5);wb(0x9866,(sample/420)&1);
  wb(0x8363,8);wb(0x8362,(sample/840)%9);wb(0x8364,(sample/7)%4);
  wb(0x8342,(sample/9)%6);wb(0x8292,(sample/11)&1);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,(sample/13+n)%4);
  ww(0x801a,(sample*4)&252);
  uint16_t d4=sample*73;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowPlayerCollision collision={sourceWord,0,0,0,0,0};
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  WillowArcadeGameplayServices services={.bus=&bus,.resolve=resolve,.word=sourceWord,
   .busWord=busRomWord,.collision=&collision,.textWrite=textWrite,.d4=&d4,.a4=&a4};
  WillowNativeGameplayFlow flow={0};
  WillowNativeGameplayFlowBinding binding={&flow,&bus,busRomWord,&a4,willowArcadeGameplayServiceResume,&services};
  uint8_t result=mode==5?willowNativeGameplayFlowService(&binding,pc,-1):
   willowArcadeGameplayServiceResume(&services,pc,sample&1?13:16);
  if(result!=1){fprintf(stderr,"Unsupported mode %u sample %u\n",mode,sample);return 1;}
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xffeac6);m68k_set_reg(10,0xff9900);m68k_set_reg(7,19);m68k_set_reg(4,(uint16_t)(sample*73));m68k_set_reg(0,sample&1?13:16);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"Handoff A4 mode %u sample %u actual %x expected %x\n",mode,sample,m68k_get_reg(12),a4);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Transition destination mismatch entry %x sample %u at %x actual %x expected %x\n",pc,sample,n,ram[n^1],expected[n]);return 1;
  }
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Transition destination D4 mismatch %u actual %x expected %x\n",sample,m68k_get_reg(4),d4);return 1;}
  checks++;
 }
 printf("Intro services/handoff: %u original comparisons; actual text/HUD, palette controller setup, stage spawns, object pool exhaustion, command queue and D4/A4 match\n",checks);
 return 0;
}
