/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_title_actor.h"
#include "../src/willow_arcade_gameplay_services.h"
#include "../src/willow_arcade_sprite_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "../src/willow_arcade_actor_pool.h"
#ifdef WILLOW_TEST_NATIVE_TITLE_DATA
#include "../native/intro_probe/src/actor_data.h"
#endif
static uint8_t rom[0x200000],expected[65536];
static WillowActorPool pool;
static uint16_t sourceWord(unsigned long at){return willowMotionWord(rom+at);}
static uint16_t busWord(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t busByte(uint16_t at){return expected[at];}
static void busPutWord(uint16_t at,uint16_t value){willowMotionPutWord(expected+at,value);}
static void busPutByte(uint16_t at,uint8_t value){expected[at]=value;}
static void releaseActor(uint8_t *record){
 willowActorPoolRelease(&pool,(record-pool.records)/128);
 uint16_t next=busWord(0x981c)-2;
 busPutWord(next,record-expected);busPutWord(0x981c,next);expected[0x9801]++;
}

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
static unsigned emitted,outputCursor;
static uint16_t pieceBudget;
static uint16_t spriteRomWord(uint32_t at){if((at&0xff0000)==0xff0000)return busWord(at);if(at+1>=sizeof rom)abort();
#ifdef WILLOW_TEST_NATIVE_TITLE_DATA
 uint16_t value=introActorWord(at);
 if(value!=willowMotionWord(rom+at)){fprintf(stderr,"Native title sprite data mismatch %x\n",at);abort();}
 return value;
#else
 return willowMotionWord(rom+at);
#endif
}
static void emitStage4Words(uint16_t x,uint16_t y,uint16_t code,uint16_t attribute)
{
 busPutWord(outputCursor,x);busPutWord(outputCursor+2,y);
 busPutWord(outputCursor+4,code);busPutWord(outputCursor+6,attribute);
 outputCursor+=8;emitted++;
}
static void emitOrdinarySprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,busWord(0x829a),pieceBudget};
 willowArcadeSprite(&writer,expected+handle,busWord(0x8408),busWord(0x840c));
 busPutWord(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void emitStage4Sprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,busWord(0x829a),pieceBudget};
 willowArcadeStage4Sprite(&writer,expected+handle,busWord(0x8408),busWord(0x840c));
 busPutWord(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void frameRamWord(uint32_t at,uint16_t value){if((at&0xff0000)!=0xff0000)abort();busPutWord(at,value);}
static uint32_t relocatedSpriteAddress(uint32_t at)
{
 if(at>=0x900000 && at<0x901000)return 0xff4000+(at-0x900000);
 if(at>=0x908000 && at<0x909000)return 0xffc000+(at-0x908000);
 abort();
}
static void writeFrameSprite(uint32_t at,uint16_t value){frameRamWord(relocatedSpriteAddress(at),value);}
static void drawFrameSprites(WillowArcadeSpriteFrame*f)
{
 uint32_t start=relocatedSpriteAddress(f->cursor);
 outputCursor=start&65535;pieceBudget=f->remaining;emitted=0;
 const WillowVisibilityBus b={busWord,busByte,busPutWord,busPutByte};
 willowArcadeVisibilityRender(&b,emitOrdinarySprite,emitStage4Sprite);
 f->cursor+=outputCursor-(start&65535);f->remaining=pieceBudget;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 willowMotionPutLong(rom+0x3b538,0xff4000);willowMotionPutLong(rom+0x3b54c,0xffc000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/title-publication-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/title-publication-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 unsigned checks=0;
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 memset(expected,0,sizeof expected);
 for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
 for(unsigned n=0;n<0x800;n++){expected[0x4000+n]=0x5a;expected[0xc000+n]=0xa5;}
 /* Follow the actual looping animation, retaining its state between frames. */
 uint8_t*r=expected+0x9dc6;memset(r,0,128);r[0]=1;r[30]=4;r[32]=8;
 willowMotionPutWord(r+6,0x4c0);willowMotionPutWord(r+10,0x40);
 busPutWord(0x8408,0x400);busPutWord(0x840c,0);
 for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
 FILE*dump=fopen("build/title-source-sprites.csv","w");if(!dump)return 2;
 fputs("frame,animation,piece,x,y,code,attribute\n",dump);
 for(unsigned frame=0;frame<512;frame++){
  /* Fixture exercises buffer selection without inventing game camera motion. */
  busPutByte(0x82a0,(frame/64)%2);ram[0x82a0^1]=busByte(0x82a0);
  willowArcadeTitleActor(r,0x9dc6,0x400,0,sourceWord,&bus,releaseActor);
  m68k_set_reg(14,0xff9dc6);RUN(0x154cc,0);
  for(unsigned n=0;n<65536;n++)if((n<0xfed0||n>=0xff04)&&ram[n^1]!=expected[n])return 1;
  unsigned sourceBase=(!busByte(0x82a0)&&!(busByte(0x8023)&128))?0xc000:0x4000;
  uint16_t d4=0;uint32_t a4=0;WillowArcadeSpriteFrame output;
  WillowArcadeGameplayServices services={.bus=&bus,.d4=&d4,.a4=&a4,
   .draw=drawFrameSprites,.spriteWord=writeFrameSprite,.frame=&output};
  if(!willowArcadeGameplayServiceResume(&services,0x3b536,-1))return 1;
  RUN(0x3b536,0);
  if(m68k_get_reg(8)!=relocatedSpriteAddress(output.cursor)||(m68k_get_reg(7)&65535)!=output.remaining)return 1;
  for(unsigned n=0;n<65536;n++)if((n<0xfed0||n>=0xff04)&&ram[n^1]!=expected[n]){
   fprintf(stderr,"Title publication frame %u address %x actual %x expected %x\n",frame,n,ram[n^1],expected[n]);return 1;
  }
  for(unsigned piece=0;piece<emitted;piece++){
   unsigned at=sourceBase+piece*8;unsigned values[4];
   for(unsigned j=0;j<4;j++)values[j]=((unsigned)ram[(at+j*2)^1]<<8)|ram[(at+j*2+1)^1];
   fprintf(dump,"%u,%u,%u,%u,%u,%u,%u\n",frame,willowMotionLong(r+20),piece,values[0],values[1],values[2],values[3]);
  }
  checks++;
 }
 fclose(dump);
 printf("Title publication: %u consecutive frames; original actor animation, visibility consumption, sprite words, stale-entry cleanup and both buffers match\n",checks);
 return 0;
}
