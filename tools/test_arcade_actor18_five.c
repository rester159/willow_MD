/* Headless differential verification of class-18 entry and shared palette allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_actor18_five.h"
#include "../src/willow_arcade_pool_init.h"
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
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}



static uint16_t sourceWord(unsigned long at){return (at&0xff0000)==0xff0000?rw(at):willowMotionWord(rom+at);}
static uint8_t*resolve(uint16_t at){return expected+at;}

static uint8_t attribute(uint16_t at){return rb(0x4000+at+2);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 if(willowMotionLong(rom+0x6772)!=0x910010)return 2;
 willowMotionPutLong(rom+0x6772,0xff4010);willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x4ad0,0xff4310);willowMotionPutLong(rom+0x4ad8,0xff4310);
 willowMotionPutLong(rom+0x3e44,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/actor18-five-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/actor18-five-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0;
 const uint32_t entries[]={0x2ac80,0x2ac80,0x2ac80};
 for(unsigned mode=0;mode<3;mode++)for(unsigned sample=0;sample<32768;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*13+sample*31)^0xa5;
  willowArcadePoolsInit(&bus);
  uint16_t actor=0xf490+(sample%8)*128;uint8_t*r=resolve(actor);
  r[0]=1;r[2]=0;r[3]=(sample%3)*2;r[30]=18;r[14]=(sample&1)*2;r[34]=(sample/5)%3;
  willowMotionPutWord(r+6,0x1700+sample%512);willowMotionPutWord(r+10,sample%300);
  ww(0x8408,0x1700);ww(0x840c,0);ww(0x8284,sample*977);wb(0x8286,sample);wb(0x8019,sample/2);
  wb(0x8470,(sample/15)%2);wb(0x838b,(sample/30)%2);wb(0x82a6,(sample/60)%2);
  wb(0xc0,(sample/120)%2);wb(0x8290,(sample/240)%2);wb(0x8293,(sample/480)%2);ww(0x801e,(sample*2)&126);
  for(unsigned n=0;n<16;n++)wb(0xf042+n*64,n<sample%17?1:0);
  r[15]=(sample/3)&1;wb(0x8346,sample);ww(0x801a,(sample*4)&252);
  willowMotionPutWord(r+56,sample%4?sample*311:0);
  r[50]=(sample/6)&1;r[48]=(sample/12)&1;r[51]=(sample/24)%3;
  r[34]=(sample/72)%3;willowMotionPutWord(r+56,(sample/216)%3);
  uint16_t x=sample*977;willowMotionPutLong(r+6,((uint32_t)x<<16)|sample);
  ww(0x984c,x+(int16_t)(sample%3)-1);
  willowMotionPutWord(r+100,x+(int16_t)((sample/648)%5)-2);
  willowMotionPutWord(r+102,x+(int16_t)((sample/3240)%5)-2);
  willowMotionPutLong(r+24,0xed068);willowActor18FourPose(r,0x2a6da,r[48],sourceWord);
  willowMotionPutWord(r+28,(sample/9720)%3);
  r[15]=sample%128;willowMotionPutLong(r+24,0xef5e4);
  if(sample%4){willowMotionPutWord(r+6,0x560+(int16_t)((sample/4)%9)-4);willowMotionPutWord(r+10,0xcd8+(int16_t)((sample/36)%9)-4);}
  if(sample%8==0)willowMotionPutWord(r+6,0x560);
  if(sample%8==1)willowMotionPutWord(r+10,0xcd8);
  wb(0x980a,(sample/324)%3);
  r[3]=10;r[4]=(sample%3)*2;r[48]=(sample/3)%9;r[67]=(sample/27)%5;
  const uint8_t timers[]={0,1,2,14,30,60,255};r[34]=timers[(sample/135)%7];r[58]=timers[(sample/945)%7];
  wb(0x980d,(sample/6615)%3);wb(0x980a,(sample/2205)%3);
  willowActor18FourPose(r,0x2a966,r[48],sourceWord);willowMotionPutWord(r+28,(sample/11025)%3);
  r[3]=12;r[4]=(sample%5)*2;r[105]=timers[(sample/4725)%7];
  willowMotionPutWord(r+96,0x560);willowMotionPutWord(r+98,0xc50);
  r[3]=8;r[4]=(sample%3)*2;
  ww(0x984c,sample*177);ww(0x9850,sample*733);
  if(sample%4==0)ww(0x984c,willowMotionWord(r+6));
  if(sample%4==1)ww(0x9850,willowMotionWord(r+10));
  if(sample%4==2){ww(0x984c,willowMotionWord(r+6)+32);ww(0x9850,willowMotionWord(r+10)+32);}
  r[3]=6;r[4]=(sample%3)*2;r[19]=3;r[15]=(sample/3)%32;
  r[50]=(sample/96)%2;r[64]=timers[(sample/192)%7];r[65]=timers[(sample/1344)%7];r[42]=(sample/9408)%3?90:24;
  uint32_t route=(r[50]?0x2a3d4:0x2a39e)+((sample/7)%7)*4;
  willowMotionPutLong(r+60,route);willowMotionPutLong(r+24,0xed864);
  if(sample%4){willowMotionPutWord(r+6,sourceWord(route)+(int16_t)((sample/4)%33)-16);willowMotionPutWord(r+10,sourceWord(route+2)+(int16_t)((sample/132)%33)-16);}
  uint32_t script=0x29a10+(int16_t)sourceWord(0x29a10+((sample/5)%4)*2);
  willowMotionPutLong(r+76,script+((sample/20)%5));
  if(sample%11==0){uint8_t command=willowActor18Byte(sourceWord,script);willowMotionPutWord(r+6,sourceWord(0x2a256+command*2));}
  for(unsigned n=0;n<0x4000;n+=4)wb(0x4000+n+2,sample+((n/4)%3)*4);
  r[2]=2;r[3]=(sample%8)*2;r[4]=((sample/8)%3)*2;
  if(r[3]==12||r[3]==14)r[4]=((sample/8)%5)*2;
  if(r[3]==2||r[3]==4){r[15]=(sample/40)%2;r[48]=(sample/80)%2;willowMotionPutLong(r+24,0xed068);willowActor18FourPose(r,0x2a6da,r[48],sourceWord);}
  r[54]=((sample/160)%4)*2;r[67]=(sample/640)%5;
  const uint8_t hp[]={0,1,24,25,90,180};r[42]=hp[(sample/3200)%6];
  r[43]=(sample/5)%2?r[42]:180;r[104]=(sample/7)%2;r[49]=timers[(sample/11)%7];
  wb(0x83f4,(sample/13)%2);
  r[2]=0;r[3]=(sample%3)*2;r[1]=(sample/3)%2;r[32]=5;r[33]=0;
  r[54]=((sample/6)%4)*2;wb(0x838a,(sample/24)%2);
  willowMotionPutWord(r+44,(sample/48)%3?64:65535);
  willowMotionPutWord(r+80,64);willowMotionPutWord(r+82,64);
  willowMotionPutWord(r+84,2);willowMotionPutWord(r+86,2);
  uint16_t freeHandles[7];unsigned freeCount=0;
  for(unsigned n=0;n<8;n++)if(0xf490+n*128!=actor)freeHandles[freeCount++]=0xf490+n*128;
  unsigned shift=(sample/96)%7;
  ww(0xf486,0xf458);wb(0xf48c,7);
  for(unsigned n=0;n<7;n++)ww(0xf458+n*2,freeHandles[(n+shift)%7]);
  const int16_t distances[]={-193,-192,-191,-1,0,1,191,192,193};
  ww(0x984c,willowMotionWord(r+6)+distances[(sample/672)%9]);
  r[2]=2;r[3]=(sample%4)*2;r[4]=((sample/4)%(r[3]==0?3:2))*2;
  r[54]=((sample/12)%4)*2;r[15]=(sample/48)%128;
  willowMotionPutWord(r+104,(sample/6144)%2);
  const uint16_t ages[]={0,1279,1280,2559,2560,65534,65535};willowMotionPutWord(r+102,ages[(sample/96)%7]);
  willowMotionPutWord(r+98,willowMotionWord(r+6)+(int16_t)((sample/7)%41)-20);
  willowMotionPutWord(r+100,willowMotionWord(r+10)+(int16_t)((sample/287)%41)-20);
  r[107]=(sample/2009)%2;r[34]=timers[(sample/13)%7];r[49]=timers[(sample/91)%7];
  r[42]=(sample/637)%3?80:0;r[43]=(sample/1911)%2?80:79;
  ww(0x838c,willowMotionWord(r+6)-32+(int16_t)(sample%3)-1);
  ww(0x838e,willowMotionWord(r+6)-352+(int16_t)((sample/3)%3)-1);
  willowPlayerAnimationSet(r,0x2b4e0,sourceWord);willowMotionPutWord(r+28,(sample/27)%3);
  r[2]=4;r[3]=(sample%3)*2;r[56]=(sample/3)%8;r[55]=timers[(sample/24)%7];r[19]=10;
  wb(0x9810,(sample/168)%8);wb(0x980d,(sample/1344)%3);
  for(unsigned n=0;n<0x4000;n+=4)wb(0x4000+n+2,sample+((n/4)%3)*4);
  r[32]=5;r[33]=mode*2;r[2]=(sample%4)*2;
  if(!mode){
   if(r[2]==0){r[3]=((sample/4)%3)*2;r[15]=(sample/12)%2;}
   if(r[2]==2){r[3]=((sample/4)%4)*2;r[4]=((sample/16)%(r[3]==0?3:2))*2;}
  }else{
   r[3]=((sample/4)%2)*2;r[15]=(sample/8)%7;
   willowMotionPutWord(r+38,0xb000);willowMotionPutWord(r+40,0xb100);
   wb(0xb002,((sample/56)%4)*2);wb(0xb06a,(sample/224)%2);wb(0xb10f,(sample/448)%7);
   ww(0xb006,sample*311);ww(0xb00a,sample*977);
  }
  uint16_t d4=sample*977;
  willowArcadeVisibilityInit(&bus,0,0);
  uint32_t initialA4=sample&1?0xffb000:0x123456,a4=initialA4;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowPlayerCollision collision={sourceWord,attribute,0,0,0,0};
  if(!willowArcadeActor18FiveWithA4(&bus,resolve,actor,sourceWord,&collision,&d4,&a4))return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(12,initialA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff0000|actor);
  m68k_set_reg(4,(uint16_t)(sample*977));m68k_set_reg(1,(uint16_t)(sample*311));m68k_set_reg(0,0);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entries[mode]);
  for(unsigned slice=0;slice<8&&m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000||(m68k_get_reg(15)&65535)!=0xff04)return 1;
  if((uint16_t)m68k_get_reg(4)!=d4){fprintf(stderr,"Child D4 mismatch mode %u sample %u\n",mode,sample);return 1;}
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Actor18 four enraged mismatch sample %u at %x actual %x expected %x\n",sample,n,ram[n^1],expected[n]);return 1;
  }
  if(m68k_get_reg(12)!=a4){fprintf(stderr,"A4 mismatch sample %u actual %x expected %x\n",sample,m68k_get_reg(12),a4);return 1;}
  checks++;
 }
 printf("Actor18 selector-five complete dispatcher: %u original non-stack RAM/D4 comparisons; all three roles, leader main states, linked actions, retirement and source side effects match\n",checks);
 return 0;
}
