/* Headless differential verification of complete 3B524 frame. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_gameplay_frame.h"
#include "../src/willow_arcade_stage_object0.h"
#include "../src/willow_arcade_sprite_writer.h"
typedef uint32_t u32;typedef uint16_t u16;typedef int16_t s16;
#include "../native/intro_probe/src/player_sprites.h"
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


static uint16_t sourceWord(uint32_t a){return (a&0xff0000)==0xff0000?rw(a):willowMotionWord(rom+a);}
static uint8_t sourceByte(uint32_t at){return rom[at];}
static uint8_t*resolve(uint16_t at){return expected+at;}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
static uint16_t colorRead(uint32_t at){if(at<0x91c000||at>=0x91d000)abort();return rw(0xe000+at-0x91c000);}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000||at>=0x91d000)abort();ww(0xe000+at-0x91c000,v);}
static uint8_t*allocate(void){for(unsigned n=0;n<16;n++)if(!rb(0xf042+n*64))return resolve(0xf042+n*64);return NULL;}
static uint8_t*allocateWithA4(uint32_t*a4){
 for(unsigned n=0;n<16;n++)if(!rb(0xf042+n*64)){if(a4)*a4=0xfff042+n*64;return resolve(0xf042+n*64);}
 if(a4)*a4=0xfff442;return NULL;
}
static void soundWithA4(uint16_t command,uint32_t*a4);
static uint8_t randomByte(void){return willowStageObjectRandom(&bus);}
static void sound(uint16_t command){
 if(rb(0x8290)&&rb(0x8293))return;
 uint16_t cursor=rw(0x801e);ww(0x8172+cursor,command);cursor=(cursor+2)&126;ww(0x8172+cursor,255);ww(0x801e,(cursor+2)&126);
}
static void soundWithA4(uint16_t command,uint32_t*a4){
 if(!(rb(0x8290)&&rb(0x8293))&&a4)*a4=0xff8172;sound(command);
}
static void mapWrite(uint16_t at,uint16_t value){if(at>=16384)abort();ww(0x4000+at,value);}
static void mapWrite3(uint16_t at,uint16_t value){if(at>=16384)abort();ww(at,value);}
static unsigned emitted,outputCursor;
static uint16_t pieceBudget,liveD4;static uint32_t liveA4;
static uint16_t spriteRomWord(uint32_t at){if((at&0xff0000)==0xff0000)return rw(at);if(at+1>=sizeof rom)abort();return willowMotionWord(rom+at);}
static void emitStage4Words(uint16_t x,uint16_t y,uint16_t code,uint16_t attribute)
{
 ww(outputCursor,x);ww(outputCursor+2,y);
 ww(outputCursor+4,code);ww(outputCursor+6,attribute);
 outputCursor+=8;emitted++;
}
static void emitOrdinarySprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,rw(0x829a),pieceBudget};
 willowArcadeSpriteWithD4(&writer,expected+handle,rw(0x8408),rw(0x840c),&liveD4);
 ww(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void emitStage4Sprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,rw(0x829a),pieceBudget};
 willowArcadeStage4SpriteWithD4(&writer,expected+handle,rw(0x8408),rw(0x840c),&liveD4);
 ww(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void frameRamWord(uint32_t at,uint16_t value){if((at&0xff0000)!=0xff0000)abort();ww(at,value);}
static uint32_t relocatedSpriteAddress(uint32_t at)
{
 if(at>=0x900000 && at<0x901000)return 0xffc000+(at-0x900000);
 if(at>=0x908000 && at<0x909000)return 0xffc800+(at-0x908000);
 abort();
}
static void writeFrameSprite(uint32_t at,uint16_t value){frameRamWord(relocatedSpriteAddress(at),value);}
static void drawFrameSprites(WillowArcadeSpriteFrame*f)
{
 uint32_t start=relocatedSpriteAddress(f->cursor);
 outputCursor=start&65535;pieceBudget=f->remaining;emitted=0;
 const WillowVisibilityBus b={rw,rb,ww,wb};
 willowArcadeVisibilityRenderWithA4(&b,emitOrdinarySprite,emitStage4Sprite,&liveA4);
 f->cursor+=outputCursor-(start&65535);f->remaining=pieceBudget;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100100,257); /* Sprite exceeding the frame area budget. */
 willowMotionPutWord(rom+0x8474,0x60fe);
 willowMotionPutWord(rom+0x65a4,0x60fe);
 willowMotionPutLong(rom+0x80,0x15d0);
 willowMotionPutLong(rom+0x9c,0x160e);
 const unsigned patches[]={0xc2e,0x1276};
 for(unsigned n=0;n<2;n++){if(willowMotionLong(rom+patches[n])!=0x91c000)return 2;willowMotionPutLong(rom+patches[n],0xffe000);}
 if(willowMotionLong(rom+0x4365a)!=0x918000)return 2;willowMotionPutLong(rom+0x4365a,0xff4000);
 if(willowMotionLong(rom+0x43d50)!=0x914000)return 2;willowMotionPutLong(rom+0x43d50,0xff0000);
 if(willowMotionWord(rom+0x3b57e)!=15)return 2;
 willowMotionPutWord(rom+0x3b57e,11);
 willowMotionPutLong(rom+0x3b538,0xffc000);willowMotionPutLong(rom+0x3b54c,0xffc800);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/gameplay-frame-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/gameplay-frame-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 unsigned checks=0;


 WillowPaletteAnimation g={0};g.byte=sourceByte;g.word=sourceWord;g.read=colorRead;g.write=colorWrite;
 g.allocateWithA4=allocateWithA4;g.soundWithA4=soundWithA4;g.allocate=allocate;g.random=randomByte;g.sound=sound;
 for(unsigned stage=0;stage<7;stage++)for(unsigned sample=0;sample<32;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+sample*31)^0xa5;
  ww(0x8284,1+sample*271);wb(0x8286,0);ww(0x801e,(sample*2)&126);
  wb(0x8290,sample&1);wb(0x8293,(sample>>1)&1);wb(0x83c3,1);
  ww(0x8408,3328);ww(0x840c,128);ww(0x8398,3072);wb(0x8395,1);wb(0x83e3,0);
  for(unsigned n=0;n<16;n++){
   uint8_t*r=resolve(0xf042+n*64);memset(r,0,64);
   r[0]=n<sample%17?1:0;r[32]=(n+sample)%8;r[60]=n%4;r[56]=n==4?10:n;r[58]=1;r[59]=3;
   willowMotionPutLong(r+52,0xea260);willowMotionPutWord(r+8,7936);willowMotionPutWord(r+12,768);
  }
  memset(resolve(0x8402),0,256);memset(resolve(0x9846),0,128);
  ww(0x8344,stage);ww(0x838c,0);ww(0x838e,7936);ww(0x8390,0);ww(0x8392,768);
  wb(0x838a,0);wb(0x838b,0);wb(0x83dd,0);wb(0x83dc,0);wb(0x83e7,0);wb(0x8362,3);
  ww(0x829c,256);ww(0x829e,256);wb(0x8023,0);
  for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
  for(unsigned step=0;step<32;step++){
  uint32_t pc=0x3b524;wb(0x83eb,step>5&&step%7==0);ww(0x8374,step%8);wb(0x8019,step);wb(0x83cf,step>=24);
  wb(0x8348,sample&1);ww(0x984c,512+sample*17+step*3);ww(0x9850,256+sample*7);
  ww(0xfb0,768+sample*13+step*2);ww(0xfb2,320+sample*3);
  if(step==16)wb(0x83e3,1);if(step==80)wb(0x83e3,0);
  wb(0x82a0,(step/8)%2);
  uint16_t cx=rw(0x8408),cy=rw(0x840c);
  for(unsigned lane=0;lane<20;lane++)for(unsigned n=0;n<(sample%4<2?0:(step+lane)%4);n++){
   uint16_t actor=0xa000+(lane*3+n)*64;uint8_t*r=resolve(actor);memset(r,0,64);
   r[0]=1;r[30]=lane<2?2:lane<5?4:lane<7?6:lane<9?8:lane<11?10:lane<14?12:lane<16?14:lane<18?16:18;
   r[14]=lane<2?lane*2:lane<5?(lane-2)*2:lane<7?(lane-5)*2:lane<9?(lane-7)*2:lane<11?(lane-9)*2:lane<14?(lane-11)*2:lane<16?(lane-14)*2:lane<18?(lane-16)*2:(lane-18)*2;
   willowMotionPutWord(r+6,cx+30+lane*17+n);willowMotionPutWord(r+10,cy+80+n*9);
   willowMotionPutLong(r+20,native_player_frames[(step+lane+n)%NATIVE_PLAYER_FRAMES].address);
   willowArcadeVisibilityQueue(r,actor,cx,cy,0,&bus);
  }
  wb(0x9846,sample%4!=0);wb(0x9847,step%3);
  willowMotionPutLong(resolve(0x985a),sample%4==1?0x100100:native_player_frames[(sample+step)%NATIVE_PLAYER_FRAMES].address);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  for(unsigned n=0;n<16;n++){
   uint16_t at=0xf042+n*64+48;uint32_t pointer=willowMotionLong(expected+at);
   if(pointer>=0x91c000&&pointer<0x91d000){pointer=pointer-0x91c000+0xffe000;for(unsigned j=0;j<4;j++)ram[(at+j)^1]=pointer>>(24-j*8);}
  }
  liveD4=sample*977+step*137;liveA4=step&1?0x123456:0xffb000;uint16_t oldD4=liveD4;uint32_t oldA4=liveA4;
  WillowArcadeSpriteFrame frame;
  if(!willowArcadeGameplayFrameWithRegisters(&bus,resolve,sourceByte,sourceWord,mapWrite,mapWrite3,NULL,NULL,&g,drawFrameSprites,writeFrameSprite,&frame,&liveD4,&liveA4))return 1;
  unsigned stop=0x100000;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  m68k_set_reg(4,oldD4);m68k_set_reg(12,oldA4);m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(14,0xff9846);m68k_set_reg(10,0xff9900);m68k_set_reg(16,pc);
  for(unsigned slice=0;slice<32&&m68k_get_reg(16)!=stop;slice++)retro_run();
  if(m68k_get_reg(16)!=stop){fprintf(stderr,"Boundary mismatch %u PC %x expected %x\n",sample,m68k_get_reg(16),stop);return 1;}
  if(m68k_get_reg(8)!=relocatedSpriteAddress(frame.cursor)||(m68k_get_reg(7)&65535)!=frame.remaining){fprintf(stderr,"Sprite output mismatch stage %u sample %u step %u\n",stage,sample,step);return 1;}
  for(unsigned n=0;n<16;n++){
   uint16_t at=0xf042+n*64+48;uint32_t pointer=0;for(unsigned j=0;j<4;j++)pointer=(pointer<<8)|ram[(at+j)^1];
   if(pointer>=0xffe000&&pointer<0xfff000){pointer=pointer-0xffe000+0x91c000;for(unsigned j=0;j<4;j++)ram[(at+j)^1]=pointer>>(24-j*8);}
  }
  for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Gameplay frame mismatch stage %u sample %u step %u at %x actual %x expected %x\n",stage,sample,step,n,ram[n^1],expected[n]);return 1;
  }

  if((uint16_t)m68k_get_reg(4)!=liveD4||m68k_get_reg(12)!=liveA4){fprintf(stderr,"Frame registers stage %u sample %u step %u D4 %x/%x A4 %x/%x\n",stage,sample,step,m68k_get_reg(4),liveD4,m68k_get_reg(12),liveA4);return 1;}
  if(sample%4<2&&emitted){fprintf(stderr,"Empty/culled coverage failure\n");return 1;}
  checks++;
 }
 }
 printf("Gameplay frame 3B524: %u complete original-pass comparisons; seven stages, both maps, palettes, 20 sprite lanes, buffer cleanup and publication match\n",checks);
 return 0;
}
