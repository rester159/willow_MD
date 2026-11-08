/* Exhaustive target-code equivalence and CPU cost, without a display. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static bool env(unsigned cmd,void*p){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)p="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)p)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)p=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)p=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT||cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*p,unsigned w,unsigned h,size_t pitch){(void)p;(void)w;(void)h;(void)pitch;}
static void audio(int16_t a,int16_t b){(void)a;(void)b;}
static size_t batch(const int16_t*p,size_t n){(void)p;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n)return 2
static unsigned symbol(const char*path,const char*name){
 FILE*f=fopen(path,"r");if(!f)return 0;char line[512],kind,key[256];unsigned at;
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&at,&kind,key)==3&&!strcmp(key,name)){fclose(f);return at;}
 fclose(f);return 0;
}
typedef struct {
 struct {void*base;unsigned(*r8)(unsigned);unsigned(*r16)(unsigned);void(*w8)(unsigned,unsigned);void(*w16)(unsigned,unsigned);}map[256];
 struct {unsigned pc,cycle,detected;}poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CpuCycles;
int main(int argc,char**argv){
 if(argc!=4){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 FILE*f=fopen(argv[2],"rb");if(!f){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}size_t bytes=fread(rom+0x100000,1,4096,f);int extra=fgetc(f);fclose(f);if(!bytes||extra!=EOF||bytes>=4094){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 unsigned entry[2]={symbol(argv[3],"slotFold"),symbol(argv[3],"slotDivide")};if(!entry[0]||!entry[1]){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100ffe);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100ffe,0x60fe);
 willowMotionPutWord(rom+0x100ffc,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/column-slot-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/column-slot-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");CpuCycles*cpu=dlsym(core,"m68k");if(!ram||!cpu){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 m68k_set_reg(16,0x100ffe);if(cpu->pc!=0x100ffe){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n)){fprintf(stderr,"Column probe setup failed at line %d\n",__LINE__);return 2;}
 m68k_set_reg(17,0x2700);m68k_set_reg(16,0x100ffc);
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);
 if(cpu->cycles!=28||m68k_get_reg(16)!=0x100ffe)return 2;
 unsigned long long total[2]={0};unsigned worst[2]={0},wins=0;
 for(int column=-32768;column<=32767;column++){
  int expected=column%43;if(expected<0)expected+=43;
  unsigned costs[2];
  for(unsigned variant=0;variant<2;variant++){
   uint8_t stack[8];willowMotionPutLong(stack,0x100ffe);willowMotionPutLong(stack+4,(int32_t)column);
   for(unsigned n=0;n<8;n++)ram[(0xff00+n)^1]=stack[n];
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry[variant]);
   cpu->cycles=0;cpu->refresh=896;
   unsigned steps=0;while(m68k_get_reg(16)!=0x100ffe&&steps++<256)m68k_run(cpu->cycles+1);
   if(steps>=256||(uint16_t)m68k_get_reg(0)!=expected||m68k_get_reg(15)!=0xffff04){fprintf(stderr,"Slot mismatch column %d variant %u PC %x result %u expected %d\n",column,variant,m68k_get_reg(16),(uint16_t)m68k_get_reg(0),expected);return 1;}
   costs[variant]=cpu->cycles;total[variant]+=costs[variant];if(costs[variant]>worst[variant])worst[variant]=costs[variant];
  }
  if(costs[0]<costs[1])wins++;
 }
 printf("Compiled column slot: all 65536 signed inputs equal modulo 43; folded mean %.2f max %u, division mean %.2f max %u master cycles; fold faster in %u cases. Function-call microbenchmark, not whole-game FPS.\n",total[0]/65536.0,worst[0],total[1]/65536.0,worst[1],wins);
 return 0;
}
