/* Exhaustive native source3B574 cleanup vs packed object-bank relocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_native_video_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],candidate[0x100000],ramBefore[65536],ramAfter[65536];
static uint8_t sramBefore[65536],sramAfter[65536];
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
static uint32_t canonical(unsigned value,unsigned packed){
 const unsigned bases[2][2]={{0x200000,0x208000},{WILLOW_VIDEO_OBJECT0,WILLOW_VIDEO_OBJECT1}};
 for(unsigned bank=0;bank<2;bank++)if(value>=bases[packed][bank]&&value<=bases[packed][bank]+0x800)return 0x900000+bank*0x8000+value-bases[packed][bank];
 return value;
}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 unsigned limit=argc==3?(unsigned)strtoul(argv[2],NULL,10):257;
 if(!limit||limit>257)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/native-video-relocation/cleanup-input.bin","rb");if(!f||fread(candidate,1,sizeof candidate,f)!=sizeof candidate)return 2;fclose(f);
 /* The complete closed routine may differ only in the proven discriminator. */
 for(unsigned at=0x3b574;at<0x3b5c6;at++)if(at!=0x3b57f&&candidate[at]!=rom[at])return 2;
 if(willowMotionWord(rom+0x3b57e)!=15||willowMotionWord(candidate+0x3b57e)!=WILLOW_VIDEO_OBJECT_BANK_BIT)return 2;
 memcpy(rom+0x100010,candidate+0x3b574,0x52);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100004,0x13fc);willowMotionPutWord(rom+0x100006,1);willowMotionPutLong(rom+0x100008,0xa130f1);willowMotionPutWord(rom+0x10000c,0x4e75);willowMotionPutWord(rom+0x10000e,0x4e71);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 f=fopen("build/native-video-relocation/cleanup-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-video-relocation/cleanup-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");CleanupCoreCycles*cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!ram||!cpu||!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100004);retro_run();if(m68k_get_reg(16)!=0x100000)return 2;
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
 unsigned checks=0;unsigned long long totalCycles=0;unsigned peak=0;
 for(unsigned screen=0;screen<2;screen++)for(unsigned bank=0;bank<2;bank++)
 for(unsigned remaining=0;remaining<limit;remaining++)for(unsigned previous=0;previous<257;previous++){
  for(unsigned n=0;n<65536;n++)ramBefore[n]=(n*37+remaining*13+previous*7+screen+bank)^0xa5;
  memset(sramBefore,0xa5,sizeof sramBefore);
  ramBefore[0x82a0]=screen;willowMotionPutWord(ramBefore+0x829c,bank?256-previous:previous);willowMotionPutWord(ramBefore+0x829e,bank?previous:256-previous);
  ramBefore[0xff00]=0;ramBefore[0xff01]=0x10;ramBefore[0xff02]=0;ramBefore[0xff03]=0;
  unsigned savedRegisters[20],cost=0;
  for(unsigned path=0;path<2;path++){
   memcpy(sram,sramBefore,65536);for(unsigned n=0;n<65536;n++)ram[n^1]=ramBefore[n];
   const unsigned bases[2][2]={{0x0000,0x8000},{WILLOW_VIDEO_OBJECT0-0x200000,WILLOW_VIDEO_OBJECT1-0x200000}};
   for(unsigned b=0;b<2;b++)for(unsigned n=0;n<0x800;n++)sram[bases[path][b]+n]=(n*11+remaining*3+previous*7+b)^0x6d;
   for(unsigned n=0;n<15;n++)m68k_set_reg(n,0x51730000+n*0x1111);
   m68k_set_reg(7,0xa7510000|remaining);m68k_set_reg(8,0x200000+bases[path][bank]+(256-remaining)*8);m68k_set_reg(13,0xff8000);
   m68k_set_reg(17,0x2715);m68k_set_reg(15,0xffff00);m68k_set_reg(16,path?0x100010:0x3b574);
   cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
   while(m68k_get_reg(16)!=0x100000&&steps++<10000)m68k_run(cpu->cycles+1);
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"No return case%u PC%x\n",checks,m68k_get_reg(16));return 1;}
   if(!path){
    memcpy(sramAfter,sram,65536);for(unsigned n=0;n<65536;n++)ramAfter[n]=ram[n^1];
    for(unsigned n=0;n<20;n++)savedRegisters[n]=canonical(m68k_get_reg(n),0);
    cost=cpu->cycles;
   }else{
    if((unsigned)cpu->cycles!=cost){fprintf(stderr,"Cycle mismatch case%u original%u packed%d\n",checks,cost,cpu->cycles);return 1;}
    for(unsigned n=0;n<20;n++)if(n!=16&&canonical(m68k_get_reg(n),1)!=savedRegisters[n]){fprintf(stderr,"Register mismatch case%u reg%u packed%x original%x\n",checks,n,canonical(m68k_get_reg(n),1),savedRegisters[n]);return 1;}
    for(unsigned n=0;n<65536;n++)if(ram[n^1]!=ramAfter[n]){fprintf(stderr,"Work RAM mismatch case%u byte%x\n",checks,n);return 1;}
    /* Every candidate SRAM byte either matches the corresponding original
     * bank byte, or its untouched sentinel outside the two object banks. */
    for(unsigned n=0;n<65536;n++){
     unsigned expectedByte=0xa5;
     if(n>=bases[1][0]&&n<bases[1][0]+0x800)expectedByte=sramAfter[bases[0][0]+n-bases[1][0]];
     if(n>=bases[1][1]&&n<bases[1][1]+0x800)expectedByte=sramAfter[bases[0][1]+n-bases[1][1]];
     if(sram[n]!=expectedByte){fprintf(stderr,"Packed SRAM mismatch case%u byte%x got%x expected%x\n",checks,n,sram[n],expectedByte);return 1;}
     if(!((n<0x800)||(n>=0x8000&&n<0x8800))&&sramAfter[n]!=0xa5){fprintf(stderr,"Original wrote outside object banks case%u byte%x\n",checks,n);return 1;}
    }
   }
  }
  if(cost>peak)peak=cost;totalCycles+=cost;checks++;
  if(!(checks%32768)){printf("Cleanup parity %u cases\n",checks);fflush(stdout);}
 }
 printf("Native source3B574 cleanup: %u cases; remaining0..%u, previous0..256, both banks and screen modes; full work RAM, all65536 SRAM bytes, canonicalized registers/CCR/SP/USP and exact CPU cycles match; mean/peak cycles %.3f/%u\n",checks,limit-1,(double)totalCycles/checks,peak);
 return 0;
}
