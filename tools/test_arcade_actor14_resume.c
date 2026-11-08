/* Headless differential verification of complete resumable class-14 scan across transition waits. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor14.h"
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
 const unsigned patches[]={0x5bd0,0x4f56,0x4f82,0x4f1a,0x4f30,0x4d6c,0x4da8,0x4fc6,0x5012,0x504c,0x5062,0x4c50,0x4a36,0x4a40,0x56ca,0x6308,0x6316,0x6320,0x632e,0x633c,0x4e34,0x4e4a,0x4e5e,0x5080,0x5084,0x5088,0x508c,0x5090,0x5094,0x5098};
 const uint32_t originals[]={0x910000,0x910070,0x9100f0,0x91140c,0x91158c,0x910df0,0x910df0,0x910170,0x910a70,0x910af0,0x910d70,0x910b70,0x91030c,0x91030c,0x9115e8,0x911388,0x910208,0x910208,0x911388,0x910a88,0x910c0c,0x910d0c,0x910e0c,0x910a70,0x910bf0,0x910cf0,0x910d70,0x910d70,0x910d70,0x910d70};
 for(unsigned n=0;n<sizeof patches/sizeof patches[0];n++){if(willowMotionLong(rom+patches[n])!=originals[n]){fprintf(stderr,"Bad relocation %x\n",patches[n]);return 2;}willowMotionPutLong(rom+patches[n],0xff4000+originals[n]-0x910000);}
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor14-resume-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor14-resume-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;

 unsigned waits=0,reloads=0,reveals=0;
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  willowArcadePoolsInit(&bus);uint16_t actor=0xdbc6+30*64;
  uint8_t*r=resolve(actor);r[0]=1;r[32]=2;r[33]=sample%11;if(r[33]==10)r[33]=11;
  willowMotionPutWord(r+6,0x200);willowMotionPutWord(r+10,0x100);
  for(unsigned n=0;n<2;n++){
   uint8_t*p=resolve(0xdbc6+(n?59:0)*64);p[0]=1;p[2]=4;p[3]=2;p[34]=255;
   willowMotionPutWord(p+6,0x200);willowMotionPutWord(p+10,0x100);
  }
  wb(0x9810,0);ww(0x8384,0);ww(0x8344,5);ww(0x8374,0);
  ww(0x984c,0x200);ww(0x9850,0x100);ww(0x8408,0x180);ww(0x840c,0x80);
  wb(0x806a,8);wb(0x8366,sample%7);wb(0x8367,(sample/7)%5);wb(0x9866,sample&1);
  wb(0x8363,8);wb(0x8362,8);wb(0x8364,sample%4);wb(0x8342,3);wb(0x8292,sample&1);
  for(unsigned n=0;n<12;n++)wb(0x8352+n,(sample+n)%4);
  wb(0x8290,sample&1);wb(0x8293,(sample/2)&1);ww(0x801e,(sample*2)&126);ww(0x801a,(sample*4)&252);
  uint32_t a4=sample&1?0xffb000:0x123456;uint16_t d4=sample*977;WillowPlayerCollision collision={sourceWord,0,0,0,0,0};
  WillowActor14Task task={0};unsigned pending=0,sourceResume=0,sourceSp=0,waitTicks=0;
  for(unsigned step=0;step<128;step++){
   if(pending){wb(0xf891,++waitTicks>=3?0:sample&1?128:255);}
   else{willowArcadeVisibilityInit(&bus,2,0);waitTicks=0;}
   uint8_t oldTimer=r[34],oldSub=r[3];uint16_t oldD4=d4;uint32_t oldA4=a4;
   for(unsigned n=0;n<65536;n++)if(!pending||n<0xfe80||n>=0xff04)ram[n^1]=expected[n];
   uint8_t result=willowArcadeActor14ResumeWithRegisters(&task,&bus,resolve,&collision,textWrite,&d4,0,&a4);
   if(!result){fprintf(stderr,"Unsupported transition sample %u step %u state %u/%u\n",sample,step,r[2],r[3]);return 1;}
   if(!pending){
    ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
    m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
    m68k_set_reg(14,0xff0000|actor);m68k_set_reg(7,59);m68k_set_reg(0,0);m68k_set_reg(4,oldD4);m68k_set_reg(16,0x2cf90);
   }else{
    /* Resume the intercepted sleep JSR; task scheduling itself is covered
     * by the separate scheduler fixtures. Preserve original caller frames. */
    m68k_set_reg(15,0xff0000|sourceSp);m68k_set_reg(16,sourceResume);
   }
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Transition boundary sample %u step %u PC %x\n",sample,step,m68k_get_reg(16));return 1;}
   if(result==2){
    unsigned sp=m68k_get_reg(15)&65535;
    sourceResume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
    sourceSp=sp+4;
    if(sourceResume!=task.transition.wait||(m68k_get_reg(0)&65535)!=1){fprintf(stderr,"Wait mismatch sample %u step %u\n",sample,step);return 1;}
    waits++;
   }else if((m68k_get_reg(15)&65535)!=0xff04){fprintf(stderr,"Return mismatch sample %u step %u\n",sample,step);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Transition mismatch sample %u step %u at %x actual %x expected %x\n",sample,step,n,ram[n^1],expected[n]);return 1;
   }
   if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 sample %u step %u actual %x expected %x\n",sample,step,m68k_get_reg(12),a4);return 1;}
   if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Transition D4 mismatch sample %u step %u actual %x expected %x\n",sample,step,m68k_get_reg(4),d4);return 1;}
   if(!pending&&oldSub==6&&oldTimer==1)reloads++;
   if(pending&&result==1&&sourceResume==0x1ec4)reveals++;
   pending=result==2;checks++;
   if(!r[0])break;
  }
 }
 if(!waits||reloads!=64||reveals!=64){fprintf(stderr,"Coverage mismatch waits %u reloads %u reveals %u\n",waits,reloads,reveals);return 1;}
 printf("Resumable actor14 scan: %u original update/wait boundaries; %u waits, %u reloads, %u reveal completions, actors before/after suspended slot, full RAM/HUD and D4/A4 match with controlled fade-completion flags\n",checks,waits,reloads,reveals);
 return 0;
}
