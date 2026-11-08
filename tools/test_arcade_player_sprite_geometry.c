/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_visibility.h"
#include "../src/willow_arcade_sprite_writer.h"
#include "../src/willow_arcade_sprite_frame.h"
#include "../src/willow_native_object_banks.h"
#include "../src/willow_arcade_actor_bounds.h"
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

static void putRamWord(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static unsigned ramWord(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
static uint8_t queueRam[65536];
static WillowNativeObjectBanks objectBanks;
static unsigned objectCapture;
static uint16_t queueWord(uint16_t at){return willowMotionWord(queueRam+at);}
static uint8_t queueByte(uint16_t at){return queueRam[at];}
static void queuePutWord(uint16_t at,uint16_t value){
 willowMotionPutWord(queueRam+at,value);
 if(objectCapture&&((at>=0x4000&&at<0x4800)||(at>=0xc000&&at<0xc800)))
  if(!willowObjectWrite(&objectBanks,0x900000UL+(at-0x4000),value))abort();
}
static void queuePutByte(uint16_t at,uint8_t value){queueRam[at]=value;}
static unsigned emitted,outputCursor;
static uint16_t pieceBudget,liveD1,liveD2,liveD3,liveD4;
static uint32_t liveA4;
static void emitQueueSprite(uint16_t handle)
{
 const uint8_t*r=queueRam+handle;
 const NativePlayerFrame*pose=NULL;
 for(unsigned n=0;n<NATIVE_PLAYER_FRAMES;n++)if(native_player_frames[n].address==willowMotionLong(r+20)){pose=native_player_frames+n;break;}
 if(!pose)abort();
 uint16_t cx=queueWord(0x8408),cy=queueWord(0x840c);
 uint16_t y=willowMotionWord(r+10)-cy;
 y=(y&0xff00)|((~y)&255);y++;
 unsigned cost=willowMotionWord(rom+pose->address);
 if(queueWord(0x829a)<cost)abort();
 queuePutWord(0x829a,queueWord(0x829a)-cost);
 queuePutWord(outputCursor,(willowMotionWord(r+6)-cx+64+pose->x)&511);
 queuePutWord(outputCursor+2,(y+pose->y)&511);
 queuePutWord(outputCursor+4,willowMotionWord(rom+pose->address+10));
 queuePutWord(outputCursor+6,willowMotionWord(rom+pose->address+2));
 outputCursor+=8;pieceBudget--;emitted++;
}
static uint16_t spriteRomWord(uint32_t at){if((at&0xff0000)==0xff0000)return queueWord(at);if(at+1>=sizeof rom)abort();return willowMotionWord(rom+at);}
static void emitStage4Words(uint16_t x,uint16_t y,uint16_t code,uint16_t attribute)
{
 queuePutWord(outputCursor,x);queuePutWord(outputCursor+2,y);
 queuePutWord(outputCursor+4,code);queuePutWord(outputCursor+6,attribute);
 outputCursor+=8;emitted++;
}
static void emitOrdinarySprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,queueWord(0x829a),pieceBudget};
 willowArcadeSpriteWithD3Tracking(&writer,queueRam+handle,queueWord(0x8408),queueWord(0x840c),&liveD1,&liveD2,&liveD4,0,&liveD3);
 queuePutWord(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void emitStage4Sprite(uint16_t handle)
{
 WillowArcadeSpriteWriter writer={spriteRomWord,emitStage4Words,queueWord(0x829a),pieceBudget};
 willowArcadeStage4SpriteWithD3Tracking(&writer,queueRam+handle,queueWord(0x8408),queueWord(0x840c),&liveD1,&liveD2,&liveD4,0,&liveD3);
 queuePutWord(0x829a,writer.areaBudget);pieceBudget=writer.pieceBudget;
}
static void frameRamWord(uint32_t at,uint16_t value){if((at&0xff0000)!=0xff0000)abort();queuePutWord(at,value);}
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
 const WillowVisibilityBus b={queueWord,queueByte,queuePutWord,queuePutByte};
 willowArcadeVisibilityRenderWithA4(&b,emitOrdinarySprite,emitStage4Sprite,&liveA4);
 f->cursor+=outputCursor-(start&65535);f->remaining=pieceBudget;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Isolate 3B536..3B554 before its first BSR. Every source instruction is
  * copied unchanged; its only relative branches remain inside the block. */
 if(willowMotionWord(rom+0x3b558)!=0x6100)return 2;
 memcpy(rom+0x100100,rom+0x3b536,0x22);
 willowMotionPutWord(rom+0x100122,0x4e75);
 /* Relocate only the two CPS sprite-RAM base operands. Both buffers retain
  * their address bit 15, which the original cleanup uses to choose history. */
 if(willowMotionLong(rom+0x3b538)!=0x900000 || willowMotionLong(rom+0x3b54c)!=0x908000)return 2;
 willowMotionPutLong(rom+0x3b538,0xff4000);willowMotionPutLong(rom+0x3b54c,0xffc000);


 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/player-sprite-geometry-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/player-sprite-geometry-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 const int cameraX[]={0,1,32,80,127,256,511,-32};
 for(unsigned c=0;c<sizeof cameraX/sizeof *cameraX;c++)for(unsigned n=0;n<NATIVE_PLAYER_FRAMES;n++){
  const NativePlayerFrame*pose=native_player_frames+n;
  memset(ram,0,65536);
  putRamWord(ram,0x1006,128);putRamWord(ram,0x100a,128);
  putRamWord(ram,0x8408,(uint16_t)cameraX[c]);
  putRamWord(ram,0x1014,pose->address>>16);putRamWord(ram,0x1016,pose->address);
  putRamWord(ram,0x829a,512);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xff2000);m68k_set_reg(9,0xff1000);m68k_set_reg(7,256);m68k_set_reg(16,0x3b880);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Sprite oracle PC %x pose %x\n",m68k_get_reg(16),pose->address);return 1;}
  unsigned expected[4]={(192-cameraX[c]+pose->x)&511,(128+pose->y)&511,willowMotionWord(rom+pose->address+10),willowMotionWord(rom+pose->address+2)};
  for(unsigned j=0;j<4;j++)if(ramWord(ram,0x2000+j*2)!=expected[j]){fprintf(stderr,"Sprite geometry mismatch pose %x camera X %d field %u\n",pose->address,cameraX[c],j);return 1;}
  if(m68k_get_reg(8)!=0xff2008 || (m68k_get_reg(7)&65535)!=255 || ramWord(ram,0x829a)!=512-willowMotionWord(rom+pose->address))return 1;
 }
 const uint16_t edges[]={0,1,31,32,272,273,416,417,448,449,65503,65504,65535};
 for(unsigned n=0;n<8192;n++){
  memset(ram,0,65536);
  uint16_t cx=n*137,cy=n*71,x=cx+edges[n%13],y=cy+edges[(n/13)%13];
  uint8_t active=n%3;
  ram[0x1000^1]=active;
  putRamWord(ram,0x1006,x);putRamWord(ram,0x100a,y);
  putRamWord(ram,0x8408,cx);putRamWord(ram,0x840c,cy);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff1000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x2244);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || ram[0x1001^1]!=willowArcadeVisible(active,x,y,cx,cy)){
   fprintf(stderr,"Source visibility mismatch case %u\n",n);return 1;
  }
 }
 unsigned boundsChecks=0;
 const unsigned boundsEntries[]={0x2162,0x2194,0x2212,0x21c6};
 for(unsigned mode=0;mode<4;mode++)for(unsigned sample=0;sample<8192;sample++){
  for(unsigned a=0;a<65536;a++)queueRam[a]=(a*73+sample*11)>>2;
  uint8_t*r=queueRam+0x1000;
  uint16_t cx=sample*137,cy=sample*71,stage=sample%6,boundary=sample*47;
  const unsigned xb[]={48,96,32},xl[]={480,576,448},yb[]={32,96,16},yl[]={320,448,288};
  if(mode<3){
   const uint16_t xe[]={-xb[mode]-1,-xb[mode],0,xl[mode]-xb[mode],xl[mode]-xb[mode]+1,65535,32768,1};
   const uint16_t ye[]={-yb[mode]-1,-yb[mode],0,yl[mode]-yb[mode],yl[mode]-yb[mode]+1,65535,32768,1};
   willowMotionPutWord(r+6,cx+xe[sample%8]);willowMotionPutWord(r+10,cy+ye[(sample/8)%8]);
  }else{
   const uint16_t xe[]={0,32767,32768,65535,1535,1536,1537,boundary-1,boundary,boundary+1};
   willowMotionPutWord(r+6,xe[(sample/6)%10]);
  }
  queuePutWord(0x8408,cx);queuePutWord(0x840c,cy);queuePutWord(0x8344,stage);queuePutWord(0x838c,boundary);
  queuePutByte(0x8395,(sample/60)%2?0x80:0);queuePutByte(0x83dd,(sample/120)%2?0xff:0);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  if(mode<3)willowArcadeActorCameraBounds(r,cx,cy,mode);
  else willowArcadeActorStageBounds(r,stage,boundary,queueByte(0x8395),queueByte(0x83dd));
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff1000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,boundsEntries[mode]);
  retro_run();if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(0)!=0)return 1;
  for(unsigned a=0;a<0xff00;a++)if(ram[a^1]!=queueRam[a]){fprintf(stderr,"Actor bounds mismatch mode %u case %u address %x\n",mode,sample,a);return 1;}
  boundsChecks++;
 }
 printf("Original actor boundary helpers: %u camera-edge, signed-stage, flag and word-wrap cases match all non-stack RAM\n",boundsChecks);
 const unsigned initEntries[]={0x27ba,0x28b8,0x290e,0x2972};
 const WillowVisibilityBus initBus={queueWord,queueByte,queuePutWord,queuePutByte};
 for(unsigned group=0;group<4;group++)for(unsigned value=0;value<256;value++){
  for(unsigned a=0;a<65536;a++)queueRam[a]=(a*73+value*11+group)>>2;
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  willowArcadeVisibilityInit(&initBus,group,value);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(1,0xcafe0000|value);m68k_set_reg(16,initEntries[group]);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  for(unsigned a=0;a<0xff00;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Visibility initialization mismatch group %u value %u address %x\n",group,value,a);return 1;
  }
 }
 puts("Original visibility initialization: 1024 cases, all four tails and every source count byte, full RAM below oracle stack matches");
 unsigned queueChecks=0;
 const WillowVisibilityBus bus={queueWord,queueByte,queuePutWord,queuePutByte};
 const unsigned entries[]={0x2244,0x2296,0x22dc};
 const unsigned pointers[]={0x9818,0x981a,0x981e,0x9820,0xf898,0x9824,0x9826,
  0x982a,0x982c,0x9830,0x9832,0x9836,0x9838,0xf900,0x983c,0x983e,
  0x9842,0x9844,0xf488,0xf48a};
 for(unsigned mode=0;mode<3;mode++)for(unsigned selector=0;selector<=18;selector+=2)
 for(unsigned lane=0;lane<((selector==4 || selector==12)?3:2);lane++)
 for(unsigned sample=0;sample<128;sample++){
  for(unsigned a=0;a<65536;a++)queueRam[a]=(a*73+sample*11)>>2;
  for(unsigned j=0;j<sizeof pointers/sizeof *pointers;j++)queuePutWord(pointers[j],0xd000+j*64+(sample%8)*2);
  uint8_t*r=queueRam+0x1000;r[0]=sample%5;r[1]=0xa7;r[30]=selector;r[14]=lane*2;
  uint16_t cx=sample*137,cy=sample*71;
  const unsigned xb[]={32,64,256},xl[]={448,512,896},yb[]={0,48,192},yl[]={272,368,640};
  const uint16_t xe[]={-xb[mode]-1,-xb[mode],0,xl[mode]-xb[mode],xl[mode]-xb[mode]+1,65535,32768,1};
  const uint16_t ye[]={-yb[mode]-1,-yb[mode],0,yl[mode]-yb[mode],yl[mode]-yb[mode]+1,65535,32768,1};
  queuePutWord(0x1006,cx+(sample<64?sample-32:xe[sample%8]));
  queuePutWord(0x100a,cy+(sample<64?sample:ye[(sample/8)%8]));
  queuePutWord(0x8408,cx);queuePutWord(0x840c,cy);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  willowArcadeVisibilityQueue(r,0x1000,cx,cy,mode,&bus);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff1000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entries[mode]);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000)return 1;
  for(unsigned a=0;a<0xff00;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Visibility queue mismatch mode %u selector %u lane %u sample %u address %x\n",mode,selector,lane,sample,a);return 1;
  }
  queueChecks++;
 }
 printf("Original visibility producers: %u cases, all three viewports and queue lanes, full RAM below oracle stack matches\n",queueChecks);
 const unsigned drainEntries[]={0x3b642,0x3b65e,0x3b67a,0x3b696,0x3b6b2,0x3b7d8,0x3b7f4,
  0x3b6ce,0x3b6ea,0x3b706,0x3b722,0x3b784,0x3b7a0,0x3b7bc,0x3b810,0x3b82c,
  0x3b848,0x3b864,0x3b73e,0x3b75a};
 const unsigned drainCounts[]={0x97ff,0x9800,0x9802,0x9803,0xf89a,0x9805,0x9806,
  0x9808,0x9809,0x980b,0x980c,0x980e,0x980f,0xf902,0x9811,0x9812,
  0x9814,0x9815,0xf48d,0xf48e};
 unsigned drainChecks=0;
 for(unsigned lane=0;lane<20;lane++)for(unsigned length=0;length<=32;length++){
  for(unsigned a=0;a<65536;a++)queueRam[a]=(a*73+length*11+lane)>>2;
  queuePutWord(pointers[lane],0xd000);queuePutByte(drainCounts[lane],length);
  queuePutWord(0x8408,length*137);queuePutWord(0x840c,length*71);
  queuePutWord(0x8344,0);queuePutWord(0x829a,4096);
  for(unsigned n=0;n<length;n++){
   unsigned actor=0xa000+n*64;
   memset(queueRam+actor,0,64);
   queuePutWord(0xd000+n*2,actor);
   queuePutWord(actor+6,100+n*13);queuePutWord(actor+10,120+n*7);
   willowMotionPutLong(queueRam+actor+20,native_player_frames[(n+length+lane)%NATIVE_PLAYER_FRAMES].address);
  }
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  emitted=0;outputCursor=0x2000;pieceBudget=256;
  willowArcadeVisibilityDrain(&bus,pointers[lane],drainCounts[lane],emitQueueSprite);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xff2000);m68k_set_reg(7,256);m68k_set_reg(16,drainEntries[lane]);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || emitted!=length || m68k_get_reg(8)!=0xff0000+outputCursor || (m68k_get_reg(7)&65535)!=pieceBudget)return 1;
  /* The sprite writer and its geometry helper use eight bytes below SP. */
  for(unsigned a=0;a<0xfef8;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Visibility drain mismatch lane %u length %u address %x\n",lane,length,a);return 1;
  }
  drainChecks++;
 }
 printf("Original visibility drains: %u ordered sprite-output and full-RAM cases across all 20 lanes (ordinary writer, stage 0)\n",drainChecks);
 unsigned ordinaryChecks=0;
 for(unsigned sample=0;sample<4096;sample++){
  memset(queueRam,0x5a,sizeof queueRam);memset(queueRam+0x1000,0,64);
  uint16_t cost=sample%9,mixed=(sample/9)%2;
  /* Synthetic input frames exercise both source data formats; the program
   * and geometry tables remain the original ROM. Mixed zero is excluded
   * because DBF represents 65,536 entries in that malformed input. */
  if(mixed && !cost)cost=1;
  uint16_t attribute=((sample/18)%4)*32 | ((sample/72)%4?((sample*0x1300)&0xff00):0) | (sample&31);
  queuePutWord(0x6000,cost|(mixed?0x8000:0));queuePutWord(0x6002,attribute);
  queuePutWord(0x6004,(sample/256)%3*128);
  for(unsigned n=0;n<32;n++)queuePutWord(0x600a+n*2,n*713+sample*37);
  uint16_t cx=sample*137,cy=sample*71;
  queuePutWord(0x1006,sample*971);queuePutWord(0x100a,sample*273);
  willowMotionPutLong(queueRam+0x1014,0xff6000);
  queuePutByte(0x1012,(sample/32)%2);queuePutWord(0x1010,sample*137);
  queuePutWord(0x8408,cx);queuePutWord(0x840c,cy);
  unsigned budget=(sample/64)%4;
  queuePutWord(0x829a,budget==0?0:budget==1 && cost?cost-1:budget==2?cost:cost+1);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  liveD1=sample*311;liveD2=sample*613;liveD3=sample*877;liveD4=sample*977;liveA4=sample&1?0x123456:0xffb000;
  emitted=0;outputCursor=0x2000;pieceBudget=sample%17;
  emitOrdinarySprite(0x1000);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*613);m68k_set_reg(3,sample*877);m68k_set_reg(4,sample*977);m68k_set_reg(12,sample&1?0x123456:0xffb000);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xff2000);m68k_set_reg(9,0xff1000);m68k_set_reg(7,sample%17);m68k_set_reg(16,0x3b880);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(8)!=0xff0000+outputCursor || (m68k_get_reg(7)&65535)!=pieceBudget){fprintf(stderr,"Ordinary sprite state mismatch sample %u\n",sample);return 1;}
  for(unsigned a=0;a<0xfefc;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Ordinary writer mismatch sample %u address %x\n",sample,a);return 1;
  }
  if((uint16_t)m68k_get_reg(3)!=liveD3){fprintf(stderr,"Sprite D3 actual %x expected %x\n",m68k_get_reg(3)&65535,liveD3);return 1;}
  if((uint16_t)m68k_get_reg(1)!=liveD1||(uint16_t)m68k_get_reg(2)!=liveD2){fprintf(stderr,"Sprite live D1/D2 sample %u actual %x/%x expected %x/%x\n",sample,m68k_get_reg(1)&65535,m68k_get_reg(2)&65535,liveD1,liveD2);return 1;}
  if((uint16_t)m68k_get_reg(4)!=liveD4||m68k_get_reg(12)!=liveA4){fprintf(stderr,"Sprite registers ordinaryChecks sample %u D4 %x/%x A4 %x/%x\n",sample,m68k_get_reg(4),liveD4,m68k_get_reg(12),liveA4);return 1;}
  ordinaryChecks++;
 }
 printf("Original 3B880 general writer: %u mixed/common-format, flip, override, wrap and budget cases match output, RAM and D1/D2/D3/D4/A4\n",ordinaryChecks);
 unsigned stage4Checks=0;
 const uint16_t stage4Edges[]={0,1,510,511,512,513,0x81fe,0x81ff,0x8200,65534,65535,32768};
 for(unsigned pose=0;pose<NATIVE_PLAYER_FRAMES;pose++)for(unsigned sample=0;sample<24;sample++)for(unsigned budget=0;budget<4;budget++){
  memset(queueRam,0x5a,sizeof queueRam);memset(queueRam+0x1000,0,64);
  uint32_t frame=native_player_frames[pose].address;
  WillowArcadeSpriteWriter geometryReader={spriteRomWord,NULL,0,0};
  unsigned geometry=willowArcadeSpriteGeometry(&geometryReader,spriteRomWord(frame+4));
  uint16_t cx=sample*137,cy=sample*71,cost=spriteRomWord(frame);
  queuePutWord(0x1006,cx-64-spriteRomWord(geometry)+stage4Edges[sample%12]);
  queuePutWord(0x100a,cy+sample*273);willowMotionPutLong(queueRam+0x1014,frame);
  queuePutByte(0x1012,sample/12);queuePutWord(0x1010,sample*137);
  queuePutWord(0x8408,cx);queuePutWord(0x840c,cy);
  queuePutWord(0x829a,budget==0?0:budget==1?cost-1:budget==2?cost:cost+1);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  liveD1=sample*311;liveD2=sample*613;liveD3=sample*877;liveD4=sample*977;liveA4=sample&1?0x123456:0xffb000;
  emitted=0;outputCursor=0x2000;pieceBudget=sample;
  emitStage4Sprite(0x1000);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*613);m68k_set_reg(3,sample*877);m68k_set_reg(4,sample*977);m68k_set_reg(12,sample&1?0x123456:0xffb000);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xff2000);m68k_set_reg(9,0xff1000);m68k_set_reg(7,sample);m68k_set_reg(16,0x3baa4);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(8)!=0xff0000+outputCursor || (m68k_get_reg(7)&65535)!=pieceBudget)return 1;
  for(unsigned a=0;a<0xfefc;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Stage 4 writer mismatch pose %u sample %u budget %u address %x\n",pose,sample,budget,a);return 1;
  }
  if((uint16_t)m68k_get_reg(3)!=liveD3){fprintf(stderr,"Sprite D3 actual %x expected %x\n",m68k_get_reg(3)&65535,liveD3);return 1;}
  if((uint16_t)m68k_get_reg(1)!=liveD1||(uint16_t)m68k_get_reg(2)!=liveD2){fprintf(stderr,"Sprite live D1/D2 sample %u actual %x/%x expected %x/%x\n",sample,m68k_get_reg(1)&65535,m68k_get_reg(2)&65535,liveD1,liveD2);return 1;}
  if((uint16_t)m68k_get_reg(4)!=liveD4||m68k_get_reg(12)!=liveA4){fprintf(stderr,"Sprite registers stage4Checks sample %u D4 %x/%x A4 %x/%x\n",sample,m68k_get_reg(4),liveD4,m68k_get_reg(12),liveA4);return 1;}
  stage4Checks++;
 }
 printf("Original 3BAA4 writer: %u pose, clipping, override, word-wrap and budget cases match sprite output, RAM and D1/D2/D3/D4/A4\n",stage4Checks);
 unsigned beginChecks=0;
 for(unsigned mode=0;mode<2;mode++)for(unsigned flags=0;flags<256;flags++){
  memset(queueRam,0x5a,sizeof queueRam);queuePutByte(0x82a0,mode);queuePutByte(0x8023,flags);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  WillowArcadeSpriteFrame frame=willowArcadeSpriteFrameBegin(&bus);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xdeadbeef);m68k_set_reg(7,0);m68k_set_reg(16,0x100100);
  retro_run();
  if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(8)!=frame.cursor || (m68k_get_reg(7)&65535)!=frame.remaining)return 1;
  for(unsigned a=0;a<0xff00;a++)if(ram[a^1]!=queueRam[a])return 1;
  beginChecks++;
 }
 printf("Original 3B536 isolated prefix: %u mode/flag cases match buffer address and initial budgets\n",beginChecks);
 unsigned clearChecks=0;
 const unsigned previousBudgets[]={0,1,128,255,256,65535};
 for(unsigned mode=0;mode<2;mode++)for(unsigned buffer=0;buffer<2;buffer++)
 for(unsigned remaining=0;remaining<=256;remaining++)for(unsigned old=0;old<6;old++){
  memset(queueRam,0x5a,sizeof queueRam);queuePutByte(0x82a0,mode);
  queuePutWord(0x829c,previousBudgets[old]);queuePutWord(0x829e,previousBudgets[old]);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  WillowArcadeSpriteFrame frame={buffer?0xffa000UL:0xff2000UL,remaining};
  willowArcadeSpriteFrameClear(&bus,&frame,frameRamWord);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,buffer?0xffa000:0xff2000);m68k_set_reg(7,remaining);m68k_set_reg(16,0x3b574);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(8)!=frame.cursor || (m68k_get_reg(7)&65535)!=frame.remaining)return 1;
  for(unsigned a=0;a<0xff00;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Sprite cleanup mismatch mode %u buffer %u remaining %u old %u address %x\n",mode,buffer,remaining,old,a);return 1;
  }
  clearChecks++;
 }
 printf("Original 3B574 cleanup: %u buffer/mode/budget cases match stale sprite words, pointer and saved budgets\n",clearChecks);
 unsigned publishChecks=0;
 for(unsigned mode=0;mode<2;mode++)for(unsigned flags=0;flags<256;flags++){
  memset(queueRam,0x5a,sizeof queueRam);queuePutByte(0x82a0,mode);queuePutByte(0x8023,flags);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  willowArcadeSpriteFramePublish(&bus);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x3b55e);
  retro_run();if(m68k_get_reg(16)!=0x100000)return 1;
  for(unsigned a=0;a<0xff00;a++)if(ram[a^1]!=queueRam[a])return 1;
  publishChecks++;
 }
 printf("Original 3B55E publication: %u mode/flag cases preserve all unrelated RAM\n",publishChecks);
 unsigned renderChecks=0;
 for(unsigned sample=0;sample<128;sample++){
  for(unsigned a=0;a<65536;a++)queueRam[a]=(a*73+sample*11)>>2;
  queuePutWord(0x8408,sample*137);queuePutWord(0x840c,sample*71);
  queuePutWord(0x8344,(sample&1)?4:0);queuePutWord(0x829a,4096);
  for(unsigned lane=0;lane<20;lane++){
   unsigned length=(sample+lane)%3;
   queuePutWord(pointers[lane],0xd000+lane*8);queuePutByte(drainCounts[lane],length);
   for(unsigned n=0;n<length;n++){
    unsigned actor=0xa000+(lane*2+n)*64;
    memset(queueRam+actor,0,64);
    queuePutWord(0xd000+lane*8+n*2,actor);
    queuePutWord(actor+6,100+lane*13+n);queuePutWord(actor+10,120+lane*7+n);
    willowMotionPutLong(queueRam+actor+20,native_player_frames[(sample+lane+n)%NATIVE_PLAYER_FRAMES].address);
   }
  }
  memset(queueRam+0x9846,0,128);
  queuePutByte(0x9846,sample%3);queuePutByte(0x9847,(sample/3)%3);
  queuePutWord(0x984c,128);
  const uint16_t playerY[]={65519,65520,0,272,273,65535,1,32768};
  queuePutWord(0x9850,queueWord(0x840c)+playerY[(sample/9)%8]);
  willowMotionPutLong(queueRam+0x985a,native_player_frames[sample%NATIVE_PLAYER_FRAMES].address);
  for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
  liveD1=sample*311;liveD2=sample*613;liveD3=sample*877;liveD4=sample*977;liveA4=sample&1?0x123456:0xffb000;
  emitted=0;outputCursor=0x2000;pieceBudget=256;
  willowArcadeVisibilityRenderWithA4(&bus,emitOrdinarySprite,emitStage4Sprite,&liveA4);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(1,sample*311);m68k_set_reg(2,sample*613);m68k_set_reg(3,sample*877);m68k_set_reg(4,sample*977);m68k_set_reg(12,sample&1?0x123456:0xffb000);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(8,0xff2000);m68k_set_reg(7,256);m68k_set_reg(16,0x3b5c6);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(8)!=0xff0000+outputCursor || (m68k_get_reg(7)&65535)!=pieceBudget)return 1;
  /* Dispatcher, lane drain, writer and geometry helper push return PCs. */
  for(unsigned a=0;a<0xfef4;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Visibility dispatch mismatch sample %u address %x\n",sample,a);return 1;
  }
  if((uint16_t)m68k_get_reg(3)!=liveD3){fprintf(stderr,"Sprite D3 actual %x expected %x\n",m68k_get_reg(3)&65535,liveD3);return 1;}
  if((uint16_t)m68k_get_reg(1)!=liveD1||(uint16_t)m68k_get_reg(2)!=liveD2){fprintf(stderr,"Sprite live D1/D2 sample %u actual %x/%x expected %x/%x\n",sample,m68k_get_reg(1)&65535,m68k_get_reg(2)&65535,liveD1,liveD2);return 1;}
  if((uint16_t)m68k_get_reg(4)!=liveD4||m68k_get_reg(12)!=liveA4){fprintf(stderr,"Sprite registers renderChecks sample %u D4 %x/%x A4 %x/%x\n",sample,m68k_get_reg(4),liveD4,m68k_get_reg(12),liveA4);return 1;}
  renderChecks++;
 }
 printf("Original 3B5C6 dispatch: %u mixed-lane and player-admission cases match ordered output, budgets, RAM and D1/D2/D3/D4/A4 (stages 0/4)\n",renderChecks);
 /* Consecutive frame sequence: preserve both output buffers, publication
  * flags, and saved budgets. Producers run independently in source/native. */
 memset(queueRam,0,sizeof queueRam);
 for(unsigned a=0x4000;a<0x4800;a++)queueRam[a]=0x5a;
 for(unsigned a=0xc000;a<0xc800;a++)queueRam[a]=0xa5;
 for(unsigned group=0;group<4;group++)willowArcadeVisibilityInit(&bus,group,0);
 for(unsigned a=0;a<65536;a++)ram[a^1]=queueRam[a];
 for(unsigned n=0;n<1024;n++){
  objectBanks.bank[0][n]=queueWord(0x4000+n*2);
  objectBanks.bank[1][n]=queueWord(0xc000+n*2);
  objectBanks.displayed[n]=n^0x7319;
 }
 uint16_t displayedBefore[1024];objectCapture=1;
 unsigned sequenceFrames=0;
 for(unsigned tick=0;tick<128;tick++){
  memcpy(displayedBefore,objectBanks.displayed,sizeof displayedBefore);
  uint16_t cx=tick*7,cy=tick*11;
  queuePutWord(0x8408,cx);queuePutWord(0x840c,cy);
  queuePutByte(0x82a0,(tick/16)%2);queuePutWord(0x8344,(tick/8)%2?4:0);
  putRamWord(ram,0x8408,cx);putRamWord(ram,0x840c,cy);
  ram[0x82a0^1]=queueByte(0x82a0);putRamWord(ram,0x8344,queueWord(0x8344));
  for(unsigned lane=0;lane<20;lane++)for(unsigned n=0;n<(tick+lane)%4;n++){
   unsigned actor=0xa000+(lane*3+n)*64;
   uint8_t*r=queueRam+actor;memset(r,0,64);
   r[0]=1;r[30]=lane<2?2:lane<5?4:lane<7?6:lane<9?8:lane<11?10:lane<14?12:lane<16?14:lane<18?16:18;
   r[14]=lane<2?lane*2:lane<5?(lane-2)*2:lane<7?(lane-5)*2:lane<9?(lane-7)*2:lane<11?(lane-9)*2:lane<14?(lane-11)*2:lane<16?(lane-14)*2:lane<18?(lane-16)*2:(lane-18)*2;
   willowMotionPutWord(r+6,cx+30+lane*17+n);willowMotionPutWord(r+10,cy+80+n*9);
   willowMotionPutLong(r+20,native_player_frames[(tick+lane+n)%NATIVE_PLAYER_FRAMES].address);
   for(unsigned j=0;j<64;j++)ram[(actor+j)^1]=r[j];
   willowArcadeVisibilityQueue(r,actor,cx,cy,0,&bus);
   putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
   m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
   m68k_set_reg(14,0xff0000+actor);m68k_set_reg(16,0x2244);retro_run();
   if(m68k_get_reg(16)!=0x100000)return 1;
  }
  uint8_t*player=queueRam+0x9846;memset(player,0,128);
  player[0]=1;player[1]=tick%3;willowMotionPutWord(player+6,cx+128);
  willowMotionPutWord(player+10,cy+128);willowMotionPutLong(player+20,native_player_frames[tick%NATIVE_PLAYER_FRAMES].address);
  for(unsigned j=0;j<128;j++)ram[(0x9846+j)^1]=player[j];
  WillowArcadeSpriteFrame frame=willowArcadeSpriteFrameRun(&bus,drawFrameSprites,writeFrameSprite);
  putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x3b536);
  for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run();
  if(m68k_get_reg(16)!=0x100000 || m68k_get_reg(8)!=relocatedSpriteAddress(frame.cursor) || (m68k_get_reg(7)&65535)!=frame.remaining){fprintf(stderr,"Frame sequence register mismatch tick %u\n",tick);return 1;}
  if((uint16_t)m68k_get_reg(3)!=liveD3){fprintf(stderr,"Sprite D3 actual %x expected %x\n",m68k_get_reg(3)&65535,liveD3);return 1;}
  if((uint16_t)m68k_get_reg(1)!=liveD1||(uint16_t)m68k_get_reg(2)!=liveD2){fprintf(stderr,"Frame sequence D1/D2 mismatch tick %u\n",tick);return 1;}
  for(unsigned a=0;a<0xfef0;a++)if(ram[a^1]!=queueRam[a]){
   fprintf(stderr,"Frame sequence mismatch tick %u address %x\n",tick,a);return 1;
  }
  for(unsigned bank=0;bank<2;bank++)for(unsigned n=0;n<1024;n++)
   if(objectBanks.bank[bank][n]!=ramWord(ram,0x4000+bank*0x8000+n*2))return 1;
  if(memcmp(displayedBefore,objectBanks.displayed,sizeof displayedBefore))return 1;
  /* An explicit modeled hardware event, not an inferred source IRQ phase. */
  unsigned selected=(queueWord(0x8022)&0x80)?1:0;
  if(!willowObjectLatch(&objectBanks,queueWord(0x8022)))return 1;
  for(unsigned n=0;n<1024;n++)
   if(objectBanks.displayed[n]!=ramWord(ram,0x4000+selected*0x8000+n*2))return 1;
  sequenceFrames++;
 }
 printf("Original 3B536 complete renderer: %u consecutive frames, independent producers, both buffers, alternating modes/stages, all output, state and D1/D2/D3 match\n",sequenceFrames);
 puts("Object-bank storage: all 4096 source bank bytes match each of 128 original renderer frames; writes preserve prior display, explicit latch copies selected 2048 bytes");
 puts("Original 2244 visibility: 8192 activity, camera-wrap and edge cases match source admission flag");
 printf("Original 3B880 sprite writer: all %u Willow poses at 8 camera X offsets match source position, code, attribute and both budgets\n",NATIVE_PLAYER_FRAMES);
 return 0;
}
