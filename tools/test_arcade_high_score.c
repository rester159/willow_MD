/* Differential verification of high-score qualification and insertion against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_high_score.h"
#include "../src/willow_native_high_scores.h"
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
static WillowNativeHighScores retainedScores;
static uint8_t rb(uint16_t a){
 if(a>=0xefc6&&a<0xf016)return retainedScores.records[a-0xefc6];
 if(a>=0xf016&&a<0xf03e)return willowHighScorePointerRead(&retainedScores,a-0xf016);
 return expected[a];
}
static uint16_t rw(uint16_t a){return ((uint16_t)rb(a)<<8)|rb(a+1);}
static void wb(uint16_t a,uint8_t v){
 if(a>=0xefc6&&a<0xf016)retainedScores.records[a-0xefc6]=v;
 if(a>=0xf016&&a<0xf03e&&!willowHighScorePointerWrite(&retainedScores,a-0xf016,v))abort();
 expected[a]=v;
}
static void ww(uint16_t a,uint16_t v){wb(a,v>>8);wb(a+1,v);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/high-score-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/high-score-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,qualified=0,ranks[11]={0};
 for(unsigned sample=0;sample<4096;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  memcpy(retainedScores.records,expected+0xefc6,80);
  memset(retainedScores.pointerBits,0,5);memset(expected+0xf016,0,40);
  uint32_t a5=(sample&2)?0xffff8000UL:0x00ff8000UL;
  for(unsigned n=0;n<10;n++)willowHighScorePutLong(&bus,0xf016+n*4,a5+0x6fc6+n*8);
  /* Ordered and deliberately unordered data: preserve the original first
   * qualifying comparison, not a newly invented sorting operation. */
  for(unsigned n=0;n<10;n++){
   uint32_t value=((sample&15)<12)?(0xfedcba98UL-n*0x11111111UL):((sample*317+n*977)^0x81234567UL);
   willowHighScorePutLong(&bus,0xefc6+n*8,value);
  }
  uint32_t score;
  if((sample&15)<12){
   unsigned mode=sample&15;
   score=mode==0?0:mode==1?0xffffffffUL:willowHighScoreLong(&bus,0xefc6+(mode-2)*8);
  }else switch(sample%4){
   case 0:score=0;break;case 1:score=0xffffffffUL;break;
   case 2:score=willowHighScoreLong(&bus,0xefc6+((sample/4)%10)*8);break;
   default:score=(sample*7919UL)^0xa59123efUL;break;
  }
  willowHighScorePutLong(&bus,0x834a,score);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  unsigned inserted=willowArcadeHighScoreQualify(&bus,a5);qualified+=inserted;
  unsigned rank=inserted?9-rw(0xf996):10;if(rank>10)return 1;ranks[rank]++;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,a5);m68k_set_reg(15,0xffff00);
  m68k_set_reg(2,0x76541234);m68k_set_reg(16,0xb954);retro_run();
  if(m68k_get_reg(16)!=0x100000||m68k_get_reg(15)!=0xffff04||m68k_get_reg(2)!=0x0000ffffUL){
   fprintf(stderr,"High-score stop mismatch sample %u PC %x D2 %x\n",sample,m68k_get_reg(16),m68k_get_reg(2));return 1;
  }
  for(unsigned n=0;n<65536;n++)if((n<0xff00||n>=0xff04)&&ram[n^1]!=rb(n)){
   fprintf(stderr,"High-score mismatch sample %u RAM %04x got %02x want %02x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  checks++;
 }
 for(unsigned n=0;n<11;n++)if(!ranks[n]){fprintf(stderr,"Missing high-score rank %u\n",n);return 1;}
 printf("High-score entry: %u original comparisons, %u qualifying insertions; all ten insertion positions and rejection, unsigned ordering/ties, retained names, shifted records, full A5-derived edit pointer and all non-stack RAM match\n",checks,qualified);
 return 0;
}
