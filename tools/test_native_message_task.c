/* Differential verification of queued text task against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_message_task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifdef WILLOW_TEST_OPENING_MESSAGE_DATA
#include "../native/intro_probe/src/opening_message_data.h"
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

static uint8_t expected[65536];
static uint16_t romWord(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t nativeWord(uint32_t at){
#ifdef WILLOW_TEST_OPENING_MESSAGE_DATA
 unsigned short value;
 if(!nativeOpeningMessageWordRead(at,&value)){fprintf(stderr,"Missing opening message word %x\n",at);abort();}
 return value;
#else
 if(at>=0x7a1e&&at<0x7ada){
  unsigned base=0x7a1e + ((at-0x7a1e)/4)*4;
  uint32_t address=((uint32_t)romWord(base)<<16)|romWord(base+2);
  address=address-0xff9000+0x910000;
  return (at-base)?address:address>>16;
 }
 return romWord(at);
#endif
}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}


static unsigned writes;
static void videoWord(uint32_t at,uint16_t value){
 if(at<0x910000||at>=0x914000){fprintf(stderr,"Text address outside map: %x\n",at);exit(1);}
 ww((uint16_t)(0x9000+at-0x910000),value);writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x1750,0x60fe);willowMotionPutWord(rom+0x1636,0x60fe);
 /* Only relocate the CPS text map; execute original stream parsing/writes. */
 /* 47 video pointers; 7ADA begins executable text code. */
 for(unsigned n=0;n<47;n++){
  unsigned at=0x7a1e + n*4;
  uint32_t address=((uint32_t)romWord(at)<<16)|romWord(at+2);
  willowMotionPutLong(rom+at,address-0x910000+0xff9000);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/message-task-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/message-task-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned checks=0,finished=0;
 const uint8_t delays[]={0,1,6,15};
 for(unsigned mode=0;mode<3;mode++)for(unsigned id=0;id<116;id++)for(unsigned layout=0;layout<3;layout++)for(unsigned d=0;d<4;d++){
#ifdef WILLOW_TEST_OPENING_MESSAGE_DATA
  if(mode||!((layout==1&&(id==0x34||id==0x35||id==0x3c||id==0x3d
#ifdef WILLOW_INTERLUDE_SETUP_PROBE
                         ||id==0x3e
#endif
                        ))||
             (layout==0&&(id==0x1b||id==0x1d))||
             (layout==2&&(id>=0x70&&id<=0x73))))continue;
#endif
#ifdef WILLOW_TEST_RESULTS_MESSAGE
  if(mode!=2||layout!=1||!(id==66||id==68||id==74||id==75||id==76||(id>=107&&id<=110)))continue;
#else
  if(mode==2)continue;
  if(id>=78&&id<112)continue;
  if(mode&&(id<64||id>65||layout!=1))continue;
#endif
  for(unsigned n=0;n<65536;n++)expected[n]=(n*7+id*31)^0xa5;
  ww(0x204,0xff);ww(0x206,0xc0);wb(0xd0,mode==2?8:mode?6:0);wb(0xd1,id);
  wb(0xd2,(layout<<4)|delays[d]);wb(0xd3,id%2?8:7);
#ifdef WILLOW_TEST_OPENING_MESSAGE_DATA
  wb(0xd3,id==0x3c?15:id==0x3d?16:id==0x1b?9:id==0x1d?14:id==0x70?10:id==0x71?11:id==0x72?12:id==0x73?13:id==0x34?7:8);
#ifdef WILLOW_INTERLUDE_SETUP_PROBE
  if(id==0x3e)wb(0xd3,0x15);
#endif
#endif
  wb(0x8293,id%2);wb(0x83c3,id%2?0:1);wb(0xe58,1);wb(0x82a6,0x77);
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  WillowNativeMessageTask task={0};task.pc=0x78f0;
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xfff000);
  m68k_set_reg(16,task.pc);
  unsigned done=0;
  for(unsigned pass=0;pass<1000;pass++){
   WillowTransitionStep step=willowNativeMessageStep(&task,&bus,0xc0,nativeWord,videoWord,NULL);
   if(!step.supported)return 1;
   retro_run();unsigned stop=m68k_get_reg(16);
   if(stop!=(step.finished?0x1636:0x1750)){fprintf(stderr,"Message stop id %u layout %u pass %u PC %x\n",id,layout,pass,stop);return 1;}
   for(unsigned n=0;n<65536;n++)if(!(n>=0xef80&&n<0xf004)&&ram[n^1]!=expected[n]){
    fprintf(stderr,"Message RAM id %u layout %u pass %u address %x actual %x expected %x\n",id,layout,pass,n,ram[n^1],expected[n]);return 1;
   }
   checks++;
   if(step.finished){finished++;done=1;break;}
   if((m68k_get_reg(0)&65535)!=step.delay)return 1;
   unsigned sp=m68k_get_reg(15)&65535;
   unsigned resume=((unsigned)ram[sp^1]<<24)|((unsigned)ram[(sp+1)^1]<<16)|((unsigned)ram[(sp+2)^1]<<8)|ram[(sp+3)^1];
   if(resume!=step.resume||m68k_get_reg(9)!=task.source||
      m68k_get_reg(10)!=task.columnBase-0x910000+0xff9000||
      m68k_get_reg(11)!=task.rowBase-0x910000+0xff9000||
      (m68k_get_reg(5)&65535)!=task.columns||(m68k_get_reg(6)&65535)!=task.rows){
    fprintf(stderr,"Message continuation mismatch id %u layout %u pass %u\n",id,layout,pass);return 1;
   }
   m68k_set_reg(15,m68k_get_reg(15)+4);m68k_set_reg(16,resume);
  }
  if(!done){fprintf(stderr,"Text failed to terminate\n");return 1;}
 }
 /* Reject unsupported modes/layouts/destinations or missing dependencies
  * before changing native context, task RAM or video. */
 for(unsigned kind=0;kind<7;kind++){
  wb(0xd0,kind==0?12:kind==6?14:0);wb(0xd2,kind==1?0x30:0x16);wb(0xd3,kind==2?63:7);
  wb(0x8293,0);wb(0x83c3,kind==6?0:1);
  WillowNativeMessageTask t={1,2,3,4,5,kind==3?0x1234:0x78f0},before=t;
  uint8_t snapshot[65536];memcpy(snapshot,expected,sizeof snapshot);unsigned oldWrites=writes;
  if(willowNativeMessageStep(&t,&bus,0xc0,kind==4?NULL:nativeWord,kind==5?NULL:videoWord,NULL).supported||
     memcmp(&t,&before,sizeof t)||memcmp(expected,snapshot,sizeof snapshot)||writes!=oldWrites)return 1;
 }
 printf("Native message task: %u source boundaries, %u streams, %u word writes match original instructions and non-stack RAM\n",checks,finished,writes);
 return 0;
}
