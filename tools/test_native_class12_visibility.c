/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_visibility.h"
#include "../src/willow_native_class12_visibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],expected[65536];
static WillowNativeClass12Visibility storage;
static uint16_t busWord(uint16_t at){uint16_t value;if(!willowNativeClass12VisibilityReadWord(&storage,at,&value))abort();return value;}
static uint8_t busByte(uint16_t at){uint8_t*p=willowNativeClass12VisibilityResolve(&storage,at);if(!p)abort();return *p;}
static void busPutWord(uint16_t at,uint16_t value){if(!willowNativeClass12VisibilityWriteWord(&storage,at,value))abort();}
static void busPutByte(uint16_t at,uint8_t value){uint8_t*p=willowNativeClass12VisibilityResolve(&storage,at);if(!p)abort();*p=value;}
static unsigned emitted;static uint16_t handles[60];
static void emit(uint16_t handle){if(emitted>=60)abort();handles[emitted++]=handle;}
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
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;

 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/class12-visibility-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/class12-visibility-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at));retro_run(); \
 if(m68k_get_reg(16)!=0x100000)return 1; \
} while(0)
 memset(&storage,0xa5,sizeof storage);memset(ram,0xa5,65536);
 willowNativeClass12VisibilityInit(&storage);
 m68k_set_reg(1,0);RUN(0x27ba,0);
 for(unsigned n=0;n<65536;n++){
  uint8_t*p=willowNativeClass12VisibilityResolve(&storage,n);
  if(p && *p!=ram[n^1]){fprintf(stderr,"Sparse queue initialization mismatch address %04x\n",n);return 1;}
 }
 const WillowVisibilityBus bus={busWord,busByte,busPutWord,busPutByte};
 const uint16_t pointers[]={0x9836,0x9838,0xf900},counts[]={0x980e,0x980f,0xf902};
 unsigned checks=0;
 for(unsigned lane=0;lane<3;lane++)for(unsigned sample=0;sample<128;sample++){
  memset(&storage,0xa5,sizeof storage);willowNativeClass12VisibilityInit(&storage);
  memset(expected,0,65536);
  for(unsigned n=0;n<65536;n++){
   uint8_t*p=willowNativeClass12VisibilityResolve(&storage,n);
   if(p)expected[n]=*p;
  }
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  unsigned inserted=0;
  for(unsigned slot=0;slot<60;slot++){
   uint16_t handle=0xccc6+slot*64;uint8_t *r=expected+handle;
   memset(r,0,64);r[0]=1;r[30]=12;r[14]=lane*2;
   uint16_t cx=sample*137,cy=sample*71;
   willowMotionPutWord(r+6,cx+(sample && (sample+slot)%5==0?449:slot));
   willowMotionPutWord(r+10,cy+(sample && (sample+slot)%7==0?273:slot));
   willowMotionPutWord(expected+0x8408,cx);willowMotionPutWord(expected+0x840c,cy);
   for(unsigned n=0;n<64;n++)ram[(handle+n)^1]=r[n];
   putRamWord(ram,0x8408,cx);putRamWord(ram,0x840c,cy);
   willowArcadeVisibilityQueue(r,handle,cx,cy,0,&bus);
   inserted+=r[1]!=0;m68k_set_reg(14,0xff0000+handle);RUN(0x2244,0);
   for(unsigned n=0;n<65536;n++)if(n<0xfed0 || n>=0xff04){
    uint8_t*p=willowNativeClass12VisibilityResolve(&storage,n);
    if(ram[n^1]!=(p?*p:expected[n])){fprintf(stderr,"Sparse queue mismatch lane %u sample %u slot %u address %04x\n",lane,sample,slot,n);return 1;}
   }
   checks++;
  }
  emitted=0;willowArcadeVisibilityDrain(&bus,pointers[lane],counts[lane],emit);
  if(emitted!=inserted || busByte(counts[lane]))return 1;
  unsigned h=0;for(int slot=59;slot>=0;slot--)if(expected[0xccc6+slot*64+1]){
   if(handles[h++]!=0xccc6+slot*64)return 1;
  }
 }
 /* Every source address either maps uniquely into the packed allocation or
  * is rejected. This also catches accidental aliasing of lane boundaries. */
 uint8_t seen[370]={0};unsigned mapped=0;
 for(unsigned at=0;at<65536;at++){
  uint8_t*p=willowNativeClass12VisibilityResolve(&storage,at);if(!p)continue;
  unsigned offset=p-storage.bytes;if(offset>=369 || seen[offset]++)return 1;mapped++;
 }
 if(mapped!=369)return 1;
 for(unsigned at=0;at<65536;at++){
  uint8_t *hi=willowNativeClass12VisibilityResolve(&storage,at);
  uint8_t *lo=willowNativeClass12VisibilityResolve(&storage,(uint16_t)(at+1));
  uint8_t valid=!(at&1) && hi && lo;
  uint16_t value=0;
  if(willowNativeClass12VisibilityReadWord(&storage,at,&value)!=valid)return 1;
  WillowNativeClass12Visibility before=storage;
  if(willowNativeClass12VisibilityWriteWord(&storage,at,0x1234)!=valid)return 1;
  if(!valid && memcmp(&before,&storage,sizeof storage))return 1;
  storage=before;
 }

 printf("Native class-12 visibility storage: %u original-producer comparisons, 384 lane drains, 369 unique mapped bytes in 370-byte allocation\n",checks);
 return 0;
}
