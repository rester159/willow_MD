/* Retained-state differential check of complete original 38B66 behavior. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_object9.h"
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
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
#ifdef WILLOW_TEST_COMPILED_STAGE9
 f=fopen("build/native_stage_object9_probe.bin","rb");if(!f)return 2;
 size_t targetBytes=fread(rom+0x110000,1,0x10000,f);int extra=fgetc(f);fclose(f);
 if(!targetBytes||extra!=EOF)return 2;
 unsigned targetEntry=0,parsed;char symbolLine[256],kind,name[128];
 f=fopen("build/native_stage_object9_probe.sym","r");if(!f)return 2;
 while(fgets(symbolLine,sizeof symbolLine,f))if(sscanf(symbolLine,"%x %c %127s",&parsed,&kind,name)==3&&!strcmp(name,"stageObject9Probe"))targetEntry=parsed;
 fclose(f);if(targetEntry<0x110000||targetEntry>=0x110000+targetBytes)return 2;
#endif
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/stage-object9-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-object9-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,spawned=0,deferred=0,failed=0,wrapped=0;

 const uint16_t differences[]={0,1,127,128,255,256,257,383,384,385,0xffff,0x8000};
 const uint16_t timers[]={0,1,2,255,256,0xffff};
 for(unsigned sample=0;sample<5184;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint16_t r=0x8502,camera=(sample*509)&65535;
  wb(r+2,0);wb(r+16,9);ww(0x8408,camera);ww(0x840c,camera+317);
  ww(r+6,camera+differences[sample%12]);ww(r+8,camera+317+differences[(sample/12)%12]);
  ww(0x8284,(sample*37)&65535);const uint8_t populations[]={0,1,3,4,5,255};wb(0x0f49,populations[(sample/864)%6]);
  wb(0x9801,(sample/37)%3);ww(0x981c,0x912a);ww(0x912a,0x9dc6);ww(0x912c,0x9e46);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t a4=0xdeadbeef;
  for(unsigned pass=0;pass<4;pass++){
   /* Inject the same timer edge into both retained executions after init. */
   if(pass==1){uint16_t value=timers[(sample/144)%6];ww(r+20,value);ram[(r+20)^1]=value>>8;ram[(r+21)^1]=value;}
   uint32_t initialA4=a4;
   unsigned oldState=rb(r+2),oldTimer=rw(r+20);
   if(!willowArcadeStageObject9WithA4(&bus,r,word,&a4))return 1;
   if(oldState==2){
    if(oldTimer==1){spawned+=rw(r+20)==256;deferred+=rw(r+20)==64;failed+=rw(r+20)==0;}
    if(!oldTimer)wrapped+=rw(r+20)==65535;
   }
   unsigned stop=0x100000;
#ifdef WILLOW_TEST_COMPILED_STAGE9
   static uint8_t originalRam[65536];memcpy(originalRam,ram,sizeof originalRam);
   uint8_t targetStack[8];willowMotionPutLong(targetStack,stop);willowMotionPutLong(targetStack+4,initialA4);
   for(unsigned j=0;j<8;j++)ram[(0xff00+j)^1]=targetStack[j];
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,targetEntry);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
   if(m68k_get_reg(16)!=stop||m68k_get_reg(0)!=a4||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"Compiled boundary mismatch %u/%u\n",sample,pass);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff08)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Compiled subtype9 mismatch %u/%u at %x\n",sample,pass,n);return 1;
   }
   memcpy(ram,originalRam,sizeof originalRam);
#endif

   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
   m68k_set_reg(14,0xff8502);m68k_set_reg(12,initialA4);m68k_set_reg(0,0);m68k_set_reg(16,0x38b66);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
   if(m68k_get_reg(16)!=stop||m68k_get_reg(12)!=a4){fprintf(stderr,"Boundary/A4 mismatch %u/%u PC %x A4 %x/%x\n",sample,pass,m68k_get_reg(16),m68k_get_reg(12),a4);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Stage object3 mismatch sample %u pass %u at %x actual %x expected %x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
  }
 }
 if(!spawned||!deferred||!failed||!wrapped)return 1;
 printf("Subtype-nine coverage: %u spawns, %u deferred attempts, %u failed allocations, %u timer wraps\n",spawned,deferred,failed,wrapped);
 printf("Stage object subtype 9: %u original RAM/A4 comparisons across 5184 retained sequences with timer-edge injection; unsigned X/Y limits, RNG, population cap, allocation failure wrap and preserved fields match\n",checks);
#ifdef WILLOW_TEST_COMPILED_STAGE9
 puts("Compiled 68000 subtype-nine execution matches every retained host/original boundary and A4 result");
#endif
 return 0;
}
