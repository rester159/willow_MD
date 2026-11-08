/* Validate source-address CPS memory callbacks on the Genesis CPU oracle. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],cps[0x20000];
static unsigned reads8,reads16,writes8,writes16;
static unsigned cpsRead8(unsigned at){if(at<0x900000||at>=0x920000){fprintf(stderr,"byte callback address%x\n",at);abort();}reads8++;return cps[at-0x900000];}
static unsigned cpsRead16(unsigned at){if(at<0x900000||at>=0x91ffff||(at&1)){fprintf(stderr,"word callback address%x\n",at);abort();}reads16++;return willowMotionWord(cps+(at-0x900000));}
static void cpsWrite8(unsigned at,unsigned v){if(at<0x900000||at>=0x920000){fprintf(stderr,"byte callback address%x\n",at);abort();}writes8++;cps[at-0x900000]=v;}
static void cpsWrite16(unsigned at,unsigned v){if(at<0x900000||at>=0x91ffff||(at&1)){fprintf(stderr,"word callback address%x\n",at);abort();}writes16++;willowMotionPutWord(cps+(at-0x900000),v);}
static bool env(unsigned cmd,void *data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT||cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CleanupCoreCycles;

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/native-video-relocation/bus-probe.bin","rb");if(!f)return 2;
 size_t size=fread(rom+0x100010,1,0xffff0,f);if(!size||!feof(f))return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutWord(rom+0x10000e,0x4e71);
 f=fopen("build/native-video-relocation/bus-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-video-relocation/bus-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");CleanupCoreCycles*cpu=dlsym(core,"m68k");if(!ram||!cpu)return 2;
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28)return 2;
 for(unsigned page=0x90;page<=0x91;page++){
  cpu->map[page].base=cps+(page-0x90)*65536;
  cpu->map[page].read8=cpsRead8;cpu->map[page].read16=cpsRead16;
  cpu->map[page].write8=cpsWrite8;cpu->map[page].write16=cpsWrite16;
 }
 memset(cps,0xa5,sizeof cps);ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=ram[0xff03^1]=0;
 for(unsigned n=0;n<4;n++)m68k_set_reg(n,0);m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100010);
 cpu->cycles=0;cpu->refresh=896;unsigned count=0;while(m68k_get_reg(16)!=0x100000&&count++<100)m68k_run(cpu->cycles+1);
 if(m68k_get_reg(16)!=0x100000||m68k_get_reg(0)!=0x12||m68k_get_reg(1)!=0x3456||m68k_get_reg(2)!=0x789abcde||m68k_get_reg(3)!=0xa55a)return 1;
 if(cps[1]!=0x12||willowMotionWord(cps+6)!=0x3456||willowMotionLong(cps+0x10010)!=0x789abcde||willowMotionWord(cps+0x1fffe)!=0xa55a)return 1;
 if(reads8!=1||writes8!=1||reads16!=4||writes16!=4)return 1;
 printf("Original CPS bus oracle: native byte/word/long instructions at90/91 pages match canonical big-endian bytes, registers and exact callbackwidthcounts; %d mastercycles\n",cpu->cycles);
 /* Native code overlays must use observed ROM page buffers and byte order.
  * Re-execute after changing MOVE.B's immediate from12 to56 outside execution. */
 uint8_t*code=cpu->map[0x10].base;
 if(code[((0x100010+3)&65535)^1]!=0x12)return 1;
 code[((0x100010+3)&65535)^1]=0x56;
 m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100010);cpu->cycles=0;cpu->refresh=896;count=0;
 while(m68k_get_reg(16)!=0x100000&&count++<100)m68k_run(cpu->cycles+1);
 if(m68k_get_reg(0)!=0x56||cps[1]!=0x56)return 1;
 puts("CPU ROM page overlay byte order and fresh-PC execution verified");
 return 0;
}
