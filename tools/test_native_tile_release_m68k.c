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
 if(argc!=4){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 FILE*f=fopen(argv[2],"rb");if(!f){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}size_t bytes=fread(rom+0x100000,1,4096,f);int extra=fgetc(f);fclose(f);if(!bytes||extra!=EOF||bytes>=4094){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 unsigned entry[2]={symbol(argv[3],"releaseTransition"),symbol(argv[3],"releaseAlways")};if(!entry[0]||!entry[1]){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100ffe);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100ffe,0x60fe);
 willowMotionPutWord(rom+0x100ffc,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/tile-release-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/tile-release-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");CpuCycles*cpu=dlsym(core,"m68k");if(!ram||!cpu){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 m68k_set_reg(16,0x100ffe);if(cpu->pc!=0x100ffe){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n)){fprintf(stderr,"Tile release probe setup failed at line %d\n",__LINE__);return 2;}
 m68k_set_reg(17,0x2700);m68k_set_reg(16,0x100ffc);
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);
 if(cpu->cycles!=28||m68k_get_reg(16)!=0x100ffe)return 2;
 unsigned long long total[2][2]={{0}};unsigned worst[2][2]={{0}},counts[2]={0};
 for(unsigned sample=0;sample<65536+1296*2;sample++){
  unsigned slot=sample%1296,bits=sample<65536?sample:sample<65536+1296?0:65535;
  unsigned word=slot>>4,group=word>>4,nonempty=!!bits;
  uint8_t initial[192]={0},expected[192]={0};
  memset(initial+176,0xa5,16);
  willowMotionPutWord(initial+word*2,bits);
  willowMotionPutWord(initial+162+group*2,bits?(1u<<(word&15)):0);
  memcpy(expected,initial,sizeof expected);
  willowMotionPutWord(expected+word*2,bits|(1u<<(slot&15)));
  willowMotionPutWord(expected+162+group*2,1u<<(word&15));
  counts[nonempty]++;
  for(unsigned variant=0;variant<2;variant++){
   for(unsigned n=0;n<sizeof initial;n++)ram[(0x2000+n)^1]=initial[n];
   uint8_t stack[8];willowMotionPutLong(stack,0x100ffe);willowMotionPutLong(stack+4,slot);
   for(unsigned n=0;n<8;n++)ram[(0xff00+n)^1]=stack[n];
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry[variant]);
   cpu->cycles=0;cpu->refresh=896;
   unsigned steps=0;while(m68k_get_reg(16)!=0x100ffe&&steps++<256)m68k_run(cpu->cycles+1);
   if(steps>=256||m68k_get_reg(15)!=0xffff04)return 1;
   for(unsigned n=0;n<sizeof expected;n++)if(ram[(0x2000+n)^1]!=expected[n]){
    fprintf(stderr,"Release mismatch sample %u variant %u offset %u\n",sample,variant,n);return 1;
   }
   unsigned cycles=cpu->cycles;total[variant][nonempty]+=cycles;
   if(cycles>worst[variant][nonempty])worst[variant][nonempty]=cycles;
  }
 }
 for(unsigned nonempty=0;nonempty<2;nonempty++)
  printf("Compiled release %s word: %u cases; transition mean %.2f/max %u, unconditional mean %.2f/max %u master cycles\n",nonempty?"nonempty":"empty",counts[nonempty],total[0][nonempty]/(double)counts[nonempty],worst[0][nonempty],total[1][nonempty]/(double)counts[nonempty],worst[1][nonempty]);
 puts("All 65536 word patterns and every slot's empty/full cases preserve exact availability, summary, cursor and guards; microbenchmark only, not FPS.");
 return 0;
}
