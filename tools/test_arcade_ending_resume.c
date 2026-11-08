/* B11A dispatcher control flow with intercepted, controlled service boundaries.
 * Service implementations are verified separately; this is not a full ending replay. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_ending_resume.h"
#include "../src/willow_arcade_stage_clear_resume.h"
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
static uint8_t expected[65536];
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(expected+a,v);}
static uint16_t rw(uint16_t a){return willowMotionWord(expected+a);}
static uint8_t rb(uint16_t a){return expected[a];}
static void wb(uint16_t a,uint8_t v){expected[a]=v;}
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static const uint32_t calls[]={0x9ef2,0x3b536,0xaf4,0xb3fa,0xbc4,0xad6,0xb04,0xb14,0xb436,0xb394,0xb2a4,0xb494,0x1e88,0xac4,0x2648,0xb4c8,0x1eaa,0xb566,0x786a,0xb690,0xb6dc,0xb710,0xb75e,0xb786,0xb7ae,0xb7dc,0xb8d8,0xb858,0x1f88};
static unsigned allowReturn,pending;static int pendingArgument;
static uint8_t service(void*context,uint32_t at,int argument){
 (void)context;
 if(allowReturn){allowReturn=0;return 1;}
 pending=at;pendingArgument=argument;return 2;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutLong(rom+0x80,0x15d0);
 for(unsigned n=0;n<sizeof calls/sizeof calls[0];n++){
  willowMotionPutWord(rom+calls[n],0x4ef9);willowMotionPutLong(rom+calls[n]+2,0x100100+n*2);
  willowMotionPutWord(rom+0x100100+n*2,0x60fe);
 }
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/ending-dispatch-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/ending-dispatch-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;
 const WillowVisibilityBus bus={rw,rb,ww,wb};unsigned checks=0;
 unsigned boundaries=0;
 for(unsigned state=0;state<=38;state+=2)for(unsigned sample=0;sample<256;sample++){
  for(unsigned n=0;n<65536;n++)expected[n]=(sample*31+n*7)^0xa5;
  ww(0x8008,20);ww(0x8344,5);
  ww(0x800c,state);ww(0x800e,sample%4);ww(0x801a,(sample%64)*4);
  wb(0xf891,(sample/4)%2);wb(0x83f6,(sample/8)%4);wb(0x83f7,(sample/16)%4);
  wb(0x0e58,(sample/32)%2);ww(0x0e8a,(sample/4)%8);wb(0x0e90,(sample/8)%4);
  wb(0x8411,sample%4);wb(0x8424,(sample/4)%4);wb(0xc0,(sample/64)%2?12:0);
  uint16_t d4=sample*977;uint32_t a4=0x123456;
  WillowArcadeEventRouteTask task={0};allowReturn=0;
  for(unsigned n=0;n<65536;n++)ram[n^1]=expected[n];
  putRamWord(ram,0xff00,0x10);putRamWord(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(4,d4);m68k_set_reg(12,a4);m68k_set_reg(16,
#ifdef WILLOW_TEST_ENDING_OUTER
  0xa5e4
#else
  0xb11a
#endif
  );
  unsigned finished=0;
  for(unsigned boundary=0;boundary<32;boundary++){
   pending=0;
   unsigned result=
#ifdef WILLOW_TEST_ENDING_OUTER
    willowArcadeStageClearResumeWithA4(&task,&bus,word,&a4,service,0);
#else
    willowArcadeEndingResume(&task,&bus,word,&a4,service,0);
#endif
   if(!result)return fprintf(stderr,"Unsupported ending dispatcher state %u sample %u PC %x\n",state,sample,task.pc),1;
   for(unsigned slice=0;slice<32&&m68k_get_reg(16)<0x100000;slice++)retro_run();
   unsigned pc=m68k_get_reg(16),sourceCall=0;
   if(pc>=0x100100&&pc<0x100100+sizeof calls/sizeof calls[0]*2)sourceCall=calls[(pc-0x100100)/2];
   if((result==1&&pc!=0x100000)||(result==2&&sourceCall!=pending))return fprintf(stderr,"Ending boundary state %u sample %u source %x native %x status %u pc %x\n",state,sample,sourceCall,pending,result,pc),1;
   if(result==2&&pendingArgument>=0&&(m68k_get_reg(0)&65535)!=(unsigned)pendingArgument)return fprintf(stderr,"Ending argument state %u sample %u\n",state,sample),1;
   for(unsigned n=0;n<65536;n++)if(!(n>=0xfe80&&n<0xff04)&&ram[n^1]!=expected[n])
    return fprintf(stderr,"Ending dispatcher %u/%u at %x RAM %x got %x want %x\n",state,sample,sourceCall,n,ram[n^1],expected[n]),1;
   if(m68k_get_reg(12)!=a4||(uint16_t)m68k_get_reg(4)!=d4)return fprintf(stderr,"Ending registers %u/%u at %x A4 %x/%x\n",state,sample,sourceCall,m68k_get_reg(12),a4),1;
   boundaries++;
   if(result==1){finished=1;break;}
   /* The oracle resumes after the intercepted service body. Display
    * segments end before their fade call; ordinary callees return via SP.
    * The controlled services intentionally make no state changes. */
   if(sourceCall==0xb4c8)m68k_set_reg(16,0xb562);
   else if(sourceCall==0xb858)m68k_set_reg(16,0xb898);
   else{
    unsigned sp=m68k_get_reg(15),ret=0;
    for(unsigned n=0;n<4;n++)ret=(ret<<8)|ram[((sp+n)&65535)^1];
    m68k_set_reg(15,sp+4);m68k_set_reg(16,ret);
   }
   allowReturn=1;
  }
  if(!finished)return 1;
  checks++;
 }
#ifdef WILLOW_TEST_ENDING_OUTER
 puts("Outer A5E4 stage-clear entry and retained ending resumes included");
#endif
 printf("Ending dispatcher: %u controlled original-instruction cases, %u service/return boundaries; all twenty state entries, source branch effects, service order/arguments, non-stack RAM and D4/A4 match. Service bodies are isolated; no complete ending replay claimed.\n",checks,boundaries);
 return 0;
}
