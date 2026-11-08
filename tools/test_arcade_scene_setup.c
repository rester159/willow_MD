/* Complete original 93D6 initialization; no service bodies instrumented. */
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
static uint32_t selectedBase;static unsigned writes,paletteWrites;
static uint8_t*resolve(uint16_t at){return expected+at;}
static uint8_t sourceByte(uint32_t at){return rom[at];}
static void videoWord(uint32_t at,uint16_t value){
 if(at>=selectedBase&&at<selectedBase+0x4000){ww(0x4000+at-selectedBase,value);writes++;}
}
static void write2(uint16_t at,uint16_t value){videoWord(0x918000+at,value);}
static void write3(uint16_t at,uint16_t value){videoWord(0x914000+at,value);}
static void colorWrite(uint32_t at,uint16_t value){ww(0x2000+at-0x91c000,value);paletteWrites++;}
int main(int argc,char**argv){
 if(argc!=3)return 2;unsigned region=strtoul(argv[2],0,10);if(region>1)return 2;
 const uint32_t bases[]={0x918000,0x914000};selectedBase=bases[region];
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 const unsigned patches[]={0x4365a,0x43d50};
 if(willowMotionLong(rom+patches[region])!=selectedBase)return 2;
 willowMotionPutLong(rom+patches[region],0xff4000);
 const unsigned palettePatches[]={0xae4,0xb06,0xaf6,0xb16};
 const uint32_t targets[]={0x91c800,0x91cc00,0x91c400,0x91c200};
 for(unsigned n=0;n<4;n++){
  if(willowMotionLong(rom+palettePatches[n])!=targets[n])return 2;
  willowMotionPutLong(rom+palettePatches[n],targets[n]-0x91c000+0xff2000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scene-setup-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scene-setup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0;
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(uint8_t)(n*7+sample*31+(n>>7));
  wb(0x828f,sample&1);wb(0x8067,((sample/2)%6)*32+(sample%32));
  ww(0x801a,(sample&63)*4);ww(0x801e,(sample&63)*2);
  ww(0x8000,2);ww(0x8004,0);ww(0x8008,0);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);ww(0x8070,2);
  ww(0x828a,sample&4?0xff:0);ww(0x828c,sample&4?0x7000:0x183c+(sample%4)*2);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t a4=0xff6780;uint16_t d4=0x3210;
  WillowPaletteAnimation palette={0};palette.write=colorWrite;
  WillowArcadeGameplayServices services={.bus=&bus,.resolve=resolve,.busWord=sourceWord,
   .byte=sourceByte,.palette=&palette,.write2=write2,.write3=write3,.d4=&d4,.a4=&a4};
  WillowNativeGameplayFlow flow={0};
  WillowNativeGameplayFlowBinding binding={&flow,&bus,sourceWord,&a4,willowArcadeGameplayServiceResume,&services};
  if(willowNativeGameplayFlowService(&binding,0x93be,-1)!=1||d4!=0x3210||flow.manager.target||flow.scene.pc||flow.scene.target)return 1;
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(12,0xff6780);m68k_set_reg(4,0x76543210);m68k_set_reg(16,0x93be);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(12)!=a4||m68k_get_reg(4)!=0x76543210||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"CPU mismatch sample %u PC %x A4 %x D4 %x\n",sample,m68k_get_reg(16),m68k_get_reg(12),m68k_get_reg(4));return 1;}
  for(unsigned n=0;n<65536;n++)if((n<0xfe80||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Init mismatch region %u sample %u at %x got %x want %x\n",region,sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 if(writes!=checks*(region?256:1024)||paletteWrites!=checks*1792)return 1;
 printf("Scene setup region %u: %u original comparisons, %u map word writes; actual palettes, four map chunks, marker allocation, pool reset and sound queue, full non-stack RAM and D4/A4 match\n",region,checks,writes);
 return 0;
}
