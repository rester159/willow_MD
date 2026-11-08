/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_scroll2_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifdef WILLOW_TEST_PACKED_MAP
#include "test_arcade_map_sink.h"
#include "../native/intro_probe/src/camera_data.h"
#endif
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
static uint16_t sourceWord(uint32_t at){if(at+1>=0x100000)abort();return willowMotionWord(rom+at);}
static uint8_t ring[16384];
static void ringWrite(uint16_t at,uint16_t value){if(at+1>=sizeof ring)abort();willowMotionPutWord(ring+at,value);}
static uint8_t sourceByte(uint32_t at){if(at>=0x100000)abort();return rom[at];}
#ifdef WILLOW_TEST_PACKED_MAP
static uint8_t cameraByte(uint32_t at){
 uint8_t value;
 if(!nativeCameraByteRead(at,&value) || value!=sourceByte(at)){fprintf(stderr,"Camera byte data mismatch %06x\n",(unsigned)at);abort();}
 return value;
}
static uint16_t cameraWord(uint32_t at){
 uint16_t value;
 if(!nativeCameraWordRead(at,&value) || value!=sourceWord(at)){fprintf(stderr,"Camera word data mismatch %06x\n",(unsigned)at);abort();}
 return value;
}
#endif

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Verified test-only CPS tile RAM operand relocation. */
 if(willowMotionLong(rom+0x4365a)!=0x918000)return 2;
 willowMotionPutLong(rom+0x4365a,0xff4000);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scroll2-active-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scroll2-active-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)

 unsigned checks[2]={0};uint32_t random=0x57494c4c;
 for(unsigned active=0;active<2;active++)for(unsigned stage=0;stage<7;stage++)for(unsigned mode=0;mode<8;mode++)for(unsigned sample=0;sample<256;sample++){
  uint8_t record[128],player[128];
  for(unsigned n=0;n<128;n++){random=random*1664525u+1013904223u;record[n]=random>>24;random=random*1664525u+1013904223u;player[n]=random>>24;}
  record[3]=mode*2;record[4]=(sample%8)*2;record[33]=(sample>>3)%4;
  record[102]=(sample>>6)&1;record[103]=sample&1;record[110]=(sample>>1)&1;
  record[64]=(sample&1)*16;record[65]=((sample>>1)&1)*16;
  record[76]=(sample&4)?4:0;record[77]=(sample&8)?12:8;
  willowMotionPutLong(record+92,0x42e90);willowMotionPutLong(record+66,0x2dc30);
  const uint16_t distances[]={0,191,192,193,194,195,32767,32768,65535};
  willowMotionPutWord(record+106,willowMotionWord(record+6)+distances[sample%9]);
  if(sample&2)willowMotionPutWord(player+6,willowMotionWord(record+104)+(int)(sample%5)-2);
  WillowScroll2Bounds g={0};g.stage=stage;g.stageX=0xabcd;g.playerY=willowMotionWord(player+10);
  g.gateA=sample&1;g.gateB=(sample>>1)&1;g.eventA=(sample>>2)&1;g.eventB=(sample>>3)&1;
  g.stage1Gate=(sample>>4)&1;g.stage1Event=(sample>>5)&1;g.hp=(sample>>2)%5;
  g.alternate=(sample>>3)&1;g.finished=sample&255;g.speed=sample*271;
  for(unsigned n=0;n<128;n++){ram[(0x8402+n)^1]=record[n];ram[(0x9846+n)^1]=player[n];}
  putRamWord(ram,0x8344,stage);putRamWord(ram,0x8398,g.stageX);
  ram[0x838a^1]=g.gateA;ram[0x838b^1]=g.gateB;ram[0x83e7^1]=g.eventA;ram[0x83cf^1]=g.eventB;
  ram[0x8395^1]=g.stage1Gate;ram[0x83dd^1]=g.stage1Event;ram[0x8362^1]=g.hp;
  ram[0x83dc^1]=g.alternate;ram[0x83d0^1]=g.finished;putRamWord(ram,0x83da,g.speed);
  for(unsigned n=0;n<sizeof ring;n++){ring[n]=(sample+n*23)&255;ram[(0x4000+n)^1]=ring[n];}
  WillowScroll2Publication pub={0};
#ifdef WILLOW_TEST_PACKED_MAP
  mapSeed(ring);
  if(active){if(!willowScroll2ActiveCopy(record,player,&g,&pub,cameraByte,cameraWord,ringWrite,mapCopy))return 2;}
#else
  if(active){if(!willowScroll2Active(record,player,&g,&pub,sourceByte,sourceWord,ringWrite))return 2;}
#endif
  else if(!willowScroll2Horizontal(record,player,&g))return 2;
#ifdef WILLOW_TEST_PACKED_MAP
  mapRead(ring);
#endif
  m68k_set_reg(14,0xff8402);RUN(active?0x4281e:0x42924,0);
  for(unsigned n=0;n<128;n++)if(record[n]!=ram[(0x8402+n)^1] || player[n]!=ram[(0x9846+n)^1]){
   fprintf(stderr,"Camera active %u stage %u mode %u case %u byte %u camera %02x/%02x player %02x/%02x\n",active,stage,mode,sample,n,record[n],ram[(0x8402+n)^1],player[n],ram[(0x9846+n)^1]);return 1;
  }
  for(unsigned n=0;n<sizeof ring;n++)if(ring[n]!=ram[(0x4000+n)^1]){
   fprintf(stderr,"Camera ring active %u stage %u mode %u case %u byte %u\n",active,stage,mode,sample,n);return 1;
  }
  if(g.gateA!=ram[0x838a^1] || g.gateB!=ram[0x838b^1] || g.finished!=ram[0x83d0^1] || g.stageX!=((ram[0x8398^1]<<8)|ram[0x8399^1]))return 1;
  if(active){
   const unsigned offsets[]={0x8032,0x8034,0x838c,0x838e,0x8390,0x8392};
   const unsigned values[]={pub.scrollX,pub.scrollY,pub.minimumX,pub.maximumX,pub.minimumY,pub.maximumY};
   for(unsigned n=0;n<6;n++)if(values[n]!=(unsigned)((ram[offsets[n]^1]<<8)|ram[(offsets[n]+1)^1]))return 1;
  }
  checks[active]++;
 }
 printf("Scroll2 horizontal dispatch: %u cases match original camera/player records and globals\n",checks[0]);
 printf("Scroll2 complete active update: %u cases match camera/player records, full tile ring, scroll registers and stage globals\n",checks[1]);
 return 0;
}
