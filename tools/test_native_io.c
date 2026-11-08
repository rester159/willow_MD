/* Entire original 18D0..1A8C I/O service vs its source-checked SRAM relocation.
 * Original-only port callbacks; target bus remains the installed Genesis core.
 * No emulated/translated gameplay behavior supplies expected results. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../build/native-video-relocation/io-test-ports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x400000],candidate[0x100000],ramBefore[65536],ramAfter[65536];
static uint8_t portsBefore[512],portsOriginal[512];
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
} IOCoreCycles;
typedef struct {unsigned pc,address,width,write,value;} IOEvent;
static IOEvent originalEvents[64],nativeEvents[64];
static unsigned originalCount,nativeCount,eventPC;
static void originalEvent(unsigned at,unsigned width,unsigned write,unsigned value){
 if(at<0x800000||at+width>0x800200||originalCount>=64)abort();
 originalEvents[originalCount++]=(IOEvent){eventPC,at,width,write,value};
}
static unsigned read8(unsigned at){if(at<0x800000||at>=0x800200)abort();unsigned v=portsOriginal[at-0x800000];originalEvent(at,1,0,v);return v;}
static unsigned read16(unsigned at){if(at<0x800000||at>0x8001fe||(at&1))abort();unsigned v=willowMotionWord(portsOriginal+(at-0x800000));originalEvent(at,2,0,v);return v;}
static void write8(unsigned at,unsigned v){originalEvent(at,1,1,v&255);portsOriginal[at-0x800000]=v;}
static void write16(unsigned at,unsigned v){if(at&1)abort();originalEvent(at,2,1,v&65535);willowMotionPutWord(portsOriginal+(at-0x800000),v);}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;
 unsigned inputLimit=argc==3?(unsigned)strtoul(argv[2],NULL,10):256;
 if(!inputLimit||inputLimit>256)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/native-video-relocation/io-test-input.bin","rb");if(!f||fread(candidate,1,sizeof candidate,f)!=sizeof candidate)return 2;fclose(f);
 if(memcmp(rom+IO_SOURCE_START,ioOriginalSpan,sizeof ioOriginalSpan)||memcmp(candidate+IO_SOURCE_START,ioNativeSpan,sizeof ioNativeSpan))return 2;
 memcpy(rom+0x100010,ioNativeSpan,sizeof ioNativeSpan);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100004,0x13fc);willowMotionPutWord(rom+0x100006,1);willowMotionPutLong(rom+0x100008,0xa130f1);willowMotionPutWord(rom+0x10000c,0x4e75);willowMotionPutWord(rom+0x10000e,0x4e71);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 f=fopen("build/native-video-relocation/io-test-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-video-relocation/io-test-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");IOCoreCycles*cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!ram||!cpu||!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 unsigned char originalMap[sizeof cpu->map[0x80]];memcpy(originalMap,cpu->map+0x80,sizeof originalMap);
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100004);retro_run();if(m68k_get_reg(16)!=0x100000)return 2;
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
 const unsigned flips[]={0,0x7f,0x80,0xff};
 const unsigned boundaries[]={0,1,0x1f,0x20,0x3f,0x40,0xff,0x100,0x1df,0x1e0,0x200,0x300,0x700,0x7fff,0x8000,0xffff};
 unsigned checks=0;unsigned long long totalCycles=0,totalEvents=0;unsigned peak=0;
 for(unsigned in=0;in<inputLimit;in++)for(unsigned flip=0;flip<16;flip++)for(unsigned edge=0;edge<16;edge++){
  unsigned seed=in*65537+flip*313+edge*1237;
  for(unsigned n=0;n<65536;n++)ramBefore[n]=(n*37+(n>>8)*17+seed)^0xa5;
  for(unsigned n=0;n<512;n++)portsBefore[n]=in*(2*n+1)+n*29+edge;
  ramBefore[0x8294]=flips[flip/4];ramBefore[0x8295]=flips[flip%4];
  for(unsigned n=0;n<10;n++){
   willowMotionPutWord(ramBefore + 0x802e + 2*n,boundaries[(edge+n)%16]);
   willowMotionPutWord(ramBefore+0xf442+2*n,boundaries[(edge+15-n)%16]);
  }
  ramBefore[0xff00]=0;ramBefore[0xff01]=0x10;ramBefore[0xff02]=ramBefore[0xff03]=0;
  unsigned savedRegisters[20],cost=0;originalCount=nativeCount=0;
  for(unsigned path=0;path<2;path++){
   memset(sram,0xa5,65536);memcpy(sram+IO_MIRROR-0x200000,portsBefore,512);memcpy(portsOriginal,portsBefore,512);
   for(unsigned n=0;n<65536;n++)ram[n^1]=ramBefore[n];
   memcpy(cpu->map+0x80,originalMap,sizeof originalMap);
   if(!path){cpu->map[0x80].base=portsOriginal;cpu->map[0x80].read8=read8;cpu->map[0x80].read16=read16;cpu->map[0x80].write8=write8;cpu->map[0x80].write16=write16;}
   for(unsigned n=0;n<15;n++)m68k_set_reg(n,0x51730000+n*0x1111+seed);
   m68k_set_reg(13,0xff8000);m68k_set_reg(17,0x2700|(checks&31));m68k_set_reg(19,0xffe780);
   m68k_set_reg(15,0xffff00);m68k_set_reg(16,path?0x100010:IO_SOURCE_START);
   cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
   while(m68k_get_reg(16)!=0x100000&&steps++<1000){
    unsigned pc=m68k_get_reg(16);eventPC=pc;
    unsigned canonical=path?pc-0x100010+IO_SOURCE_START:pc,index=0;
    if(canonical<IO_SOURCE_START||canonical>=IO_SOURCE_END){fprintf(stderr,"IO test escaped closed source at%x\n",pc);return 1;}
    for(;index<sizeof ioPorts/sizeof ioPorts[0];index++)if(ioPorts[index].pc==canonical)break;
    unsigned value=0;
    if(path&&index<sizeof ioPorts/sizeof ioPorts[0]&&!ioPorts[index].write){
     const uint8_t*p=sram+(ioPorts[index].target-0x200000);value=ioPorts[index].width==1?*p:willowMotionWord(p);
    }
    m68k_run(cpu->cycles+1);
    if(path&&index<sizeof ioPorts/sizeof ioPorts[0]){
     if(ioPorts[index].write){const uint8_t*p=sram+(ioPorts[index].target-0x200000);value=ioPorts[index].width==1?*p:willowMotionWord(p);}
     if(nativeCount>=64)abort();nativeEvents[nativeCount++]=(IOEvent){canonical,ioPorts[index].source,ioPorts[index].width,ioPorts[index].write,value};
    }
   }
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"IO routine no return case%u PC%x\n",checks,m68k_get_reg(16));return 1;}
   if(!path){
    for(unsigned n=0;n<65536;n++)ramAfter[n]=ram[n^1];
    for(unsigned n=0;n<20;n++)savedRegisters[n]=m68k_get_reg(n);
    cost=cpu->cycles;
    /* Preserve oracle final ports while resetting independent candidate inputs. */
    memcpy(candidate,portsOriginal,512);
   }else{
    if((unsigned)cpu->cycles!=cost){fprintf(stderr,"IO cycle mismatch case%u original%u native%d\n",checks,cost,cpu->cycles);return 1;}
    for(unsigned n=0;n<20;n++)if(m68k_get_reg(n)!=savedRegisters[n]){fprintf(stderr,"IO register mismatch case%u reg%u\n",checks,n);return 1;}
    for(unsigned n=0;n<65536;n++)if(ram[n^1]!=ramAfter[n]){fprintf(stderr,"IO RAM mismatch case%u at%x\n",checks,n);return 1;}
    for(unsigned n=0;n<65536;n++){
     unsigned v=n>=IO_MIRROR-0x200000&&n<IO_MIRROR-0x200000+512?candidate[n-(IO_MIRROR-0x200000)]:0xa5;
     if(sram[n]!=v){fprintf(stderr,"IO SRAM mismatch case%u at%x\n",checks,n);return 1;}
    }
    if(originalCount!=26||nativeCount!=originalCount||memcmp(originalEvents,nativeEvents,sizeof(IOEvent)*originalCount)){
     fprintf(stderr,"IO ordered event mismatch case%u original%u native%u\n",checks,originalCount,nativeCount);return 1;
    }
   }
  }
  if(cost>peak)peak=cost;totalCycles+=cost;totalEvents+=originalCount;checks++;
  if(!(checks%8192)){printf("Native IO parity %u cases\n",checks);fflush(stdout);}
 }
 printf("Native source18D0..1A8C IO: %u cases; %u raw input/DIP values each, 16 flip combinations, 16 scroll boundary rotations; full work RAM, all65536 SRAM bytes, every register/CCR/SR/SP/USP, %llu ordered port events and exact cycles match; mean/peak cycles %.3f/%u\n",checks,inputLimit,totalEvents,(double)totalCycles/checks,peak);
 return 0;
}
