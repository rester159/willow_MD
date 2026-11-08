/* Retained-state differential check of complete original 3833C behavior. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_stage_object0.h"
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
#ifdef WILLOW_TEST_COMPILED_STAGE0_ALTERNATE
 f=fopen("build/native_stage_object0_alternate_probe.bin","rb");if(!f)return 2;
 size_t targetBytes=fread(rom+0x110000,1,0x10000,f);int extra=fgetc(f);fclose(f);
 if(!targetBytes||extra!=EOF)return 2;
 unsigned targetEntry=0,parsed;char symbolLine[256],kind,name[128];
 f=fopen("build/native_stage_object0_alternate_probe.sym","r");if(!f)return 2;
 while(fgets(symbolLine,sizeof symbolLine,f))if(sscanf(symbolLine,"%x %c %127s",&parsed,&kind,name)==3&&!strcmp(name,"stageObject0Probe"))targetEntry=parsed;
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
 f=fopen("build/stage-object0_alternate-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/stage-object0_alternate-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,spawned[2]={0},failed=0,wrapped=0,retired=0,paired=0;
 const uint16_t edges[]={0,1,255,256,383,384,0x8000,0xffff};
 for(unsigned sample=0;sample<8192;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  uint16_t r=0x8502;uint8_t ordinal=0,mode=(sample/4)%2?4:2;
  uint32_t sourceRecord=0xffff0000UL|r;
  wb(r+2,sample%4*2);wb(r+16,0);wb(r+17,mode);
  wb(0x8346,sample/7);ww(0x8344,sample%6);
  uint16_t camera=sample*13;ww(0x8408,camera);ww(0x840c,sample*23);ww(0x9850,sample*31);
  ww(r+6,camera+edges[(sample/17)%8]);ww(r+10,camera-edges[(sample/31)%8]);ww(r+12,sample*79);
  if(mode==2)ww(r+20,(sample/11)%3);else wb(r+20,(sample/11)%3);
  wb(0x8395,(sample/13)%2);wb(0x83d0,(sample/19)%2);wb(0x0f40,(sample/23)%9);
  wb(0x9802,(sample/29)%2);wb(0x9803,(sample/41)%2);wb(0xf89a,(sample/43)%2);ww(0x988e,(sample/47)%2);
  wb(0x9801,(sample/37)%3);ww(0x981c,0x912a);ww(0x912a,0x9dc6);ww(0x912c,0x9e46);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  uint32_t a4=0xdeadbeef;
  for(unsigned pass=0;pass<4;pass++){
   uint32_t initialA4=a4;
   unsigned oldState=rb(r+2),oldAvailable=rb(0x9801),oldTimer=mode==2?rw(r+20):rb(r+20);
   unsigned eligible=oldState==2&&oldTimer==1&&(mode==4||(!rb(0x8395)&&!rb(0x83d0)&&!rb(0x9802)&&!rb(0x9803)&&!rb(0xf89a)));
   failed+=eligible&&!oldAvailable;
   if(!willowArcadeStageObject0WithA4(&bus,r,word,&a4))return 1;
   spawned[mode==4]+=oldAvailable-rb(0x9801);paired+=oldAvailable-rb(0x9801)==2;
   retired+=oldState>=4;
   wrapped+=!oldTimer&&(mode==2?rw(r+20)==65535:rb(r+20)==255);
   unsigned stop=0x100000;
#ifdef WILLOW_TEST_COMPILED_STAGE0_ALTERNATE
   static uint8_t originalRam[65536];memcpy(originalRam,ram,sizeof originalRam);
   uint8_t targetStack[16];willowMotionPutLong(targetStack,stop);willowMotionPutLong(targetStack+4,initialA4);willowMotionPutLong(targetStack+8,sourceRecord);willowMotionPutLong(targetStack+12,ordinal);
   for(unsigned j=0;j<16;j++)ram[(0xff00+j)^1]=targetStack[j];
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,targetEntry);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
   if(m68k_get_reg(16)!=stop||m68k_get_reg(0)!=a4||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"Compiled boundary mismatch %u/%u\n",sample,pass);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff10)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Compiled subtype20 mismatch %u/%u at %x\n",sample,pass,n);return 1;
   }
   memcpy(ram,originalRam,sizeof originalRam);
#endif

   ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
   m68k_set_reg(14,sourceRecord);m68k_set_reg(7,ordinal);m68k_set_reg(12,initialA4);m68k_set_reg(0,0);m68k_set_reg(16,0x3833c);
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
   if(m68k_get_reg(16)!=stop||m68k_get_reg(12)!=a4){fprintf(stderr,"Boundary/A4 mismatch %u/%u PC %x A4 %x/%x\n",sample,pass,m68k_get_reg(16),m68k_get_reg(12),a4);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Stage object0_alternate mismatch sample %u pass %u at %x actual %x expected %x\n",sample,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
  }
 }
 if(!spawned[0]||!spawned[1]||!failed||!paired||!wrapped||!retired)return 1;
 printf("Subtype-zero alternate coverage: %u/%u actors spawned, %u failed attempts, %u paired spawns, %u wraps, %u retirements\n",spawned[0],spawned[1],failed,paired,wrapped,retired);
 printf("Stage object zero variants 2/4: %u original RAM/A4 boundaries across 8192 retained sequences match\n",checks);
#ifdef WILLOW_TEST_COMPILED_STAGE0_ALTERNATE
 puts("Compiled 68000 subtype-zero alternate execution matches every retained host/original boundary and A4 result");
#endif
 return 0;
}
