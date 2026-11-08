/* Closed original690C transition routine vs address-only native relocation.
 * The test-only20C800 allocation replaces the displayed-object latch. It is
 * NOT a simultaneously admitted video layout or production memory allocation. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#define SOURCE_BASE 0x922000u
#define TARGET_BASE 0x20c800u
#define TABLE_SIZE 0x800u
static uint8_t rom[0x400000],table[TABLE_SIZE],before[65536],after[65536];
static unsigned currentPC,writes,low,high;
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
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n){fprintf(stderr,"Missing core symbol %s\n",#n);return 2;}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CoreCycles;
static void bounds(unsigned at,unsigned width){
 if(at<SOURCE_BASE||at+width>SOURCE_BASE+TABLE_SIZE){fprintf(stderr,"Unmapped source access%x/%u PC%x\n",at,width,currentPC);abort();}
}
static unsigned read8(unsigned at){bounds(at,1);return table[at-SOURCE_BASE];}
static unsigned read16(unsigned at){bounds(at,2);if(at&1)abort();return willowMotionWord(table+(at-SOURCE_BASE));}
static void write8(unsigned at,unsigned value){bounds(at,1);(void)value;fprintf(stderr,"Unexpected byte write%x PC%x\n",at,currentPC);abort();}
static void write16(unsigned at,unsigned value){
 bounds(at,2);if(at&1)abort();willowMotionPutWord(table+(at-SOURCE_BASE),value);
 if(at<low)low=at;if(at+2>high)high=at+2;writes++;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 if(willowMotionLong(rom+0x6982)!=SOURCE_BASE||willowMotionWord(rom+0x6980)!=0x207c)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100004,0x13fc);willowMotionPutWord(rom+0x100006,1);willowMotionPutLong(rom+0x100008,0xa130f1);willowMotionPutWord(rom+0x10000c,0x4e75);willowMotionPutWord(rom+0x10000e,0x4e71);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 f=fopen("build/native-rowscroll/closed-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));FN(size_t,retro_get_memory_size,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-rowscroll/closed-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 uint8_t*ram=dlsym(core,"work_ram");CoreCycles*cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
 if(!ram||!cpu||!sram||retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)<65536)return 2;
 unsigned char savedMap[sizeof cpu->map[0]];memcpy(savedMap,cpu->map+0x92,sizeof savedMap);
 ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
 m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x100004);retro_run();if(m68k_get_reg(16)!=0x100000)return 2;
 m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
 const unsigned counters[]={0,1,2,3,0xfc,0xfd,0xfe,0xff,0x100,0x101,0x102,0x103,0x1fe,0x1ff,0x200,0x201,0x7ffc,0x7ffd,0x7ffe,0x7fff,0x8000,0x8001,0x8002,0x8003,0xfff8,0xfff9,0xfffa,0xfffb,0xfffc,0xfffd,0xfffe,0xffff};
 unsigned checks=0,negative=0,peak=0;unsigned long long cycles=0,totalWrites=0;
 for(unsigned transition=0;transition<256;transition++)for(unsigned variant=0;variant<32;variant++){
  for(unsigned n=0;n<65536;n++)before[n]=(n*37+transition*13+variant*7)^0xa5;
  before[0x83d6]=transition;willowMotionPutWord(before+0x8042,counters[variant]);
  willowMotionPutWord(before+0x8044,0x5555u^(variant*0x1111u));willowMotionPutWord(before+0x804a,0xaaaau^(variant*0x1111u));
  willowMotionPutLong(before+0xff00,0x100000);
  unsigned regs[21],cost=0;
  for(unsigned path=0;path<2;path++){
   uint8_t*program=cpu->map[0].base;if(!program)return 2;unsigned pointer=path?TARGET_BASE:SOURCE_BASE;
   for(unsigned n=0;n<4;n++)program[(0x6982+n)^1]=pointer>>(24-n*8);
   memcpy(cpu->map+0x92,savedMap,sizeof savedMap);
   if(!path){cpu->map[0x92].base=table;cpu->map[0x92].read8=read8;cpu->map[0x92].read16=read16;cpu->map[0x92].write8=write8;cpu->map[0x92].write16=write16;memset(table,0xa5,sizeof table);writes=0;low=~0u;high=0;}
   memset(sram,0xa5,65536);for(unsigned n=0;n<65536;n++)ram[n^1]=before[n];
   for(unsigned n=0;n<15;n++)m68k_set_reg(n,0x71510000+n*0x1111);
   m68k_set_reg(13,0xff8000);m68k_set_reg(19,0xff0f00);m68k_set_reg(17,0x2700|variant);m68k_set_reg(15,0xffff00);m68k_set_reg(16,0x690c);
   cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
   while(m68k_get_reg(16)!=0x100000&&steps++<20000){currentPC=m68k_get_reg(16);m68k_run(cpu->cycles+1);}
   if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"No return case%u PC%x\n",checks,m68k_get_reg(16));return 1;}
   if(!path){
    for(unsigned n=0;n<65536;n++)after[n]=ram[n^1];for(unsigned n=0;n<21;n++)regs[n]=m68k_get_reg(n);cost=cpu->cycles;
    if(transition&0x80){if(writes!=1024||low!=SOURCE_BASE||high!=SOURCE_BASE+TABLE_SIZE){fprintf(stderr,"Incorrect write footprint\n");return 1;}negative++;}
    else if(writes){fprintf(stderr,"Nonnegative branch touched rowscroll\n");return 1;}
    totalWrites+=writes;
   }else{
    if((unsigned)cpu->cycles!=cost){fprintf(stderr,"Cycle mismatch case%u\n",checks);return 1;}
    for(unsigned n=0;n<21;n++){unsigned value=m68k_get_reg(n);if(n==8&&(transition&0x80)&&value==TARGET_BASE+TABLE_SIZE)value=SOURCE_BASE+TABLE_SIZE;if(value!=regs[n]){fprintf(stderr,"Register mismatch case%u r%u %x/%x\n",checks,n,value,regs[n]);return 1;}}
    for(unsigned n=0;n<65536;n++){
     if(ram[n^1]!=after[n]){fprintf(stderr,"Work RAM mismatch case%u byte%x\n",checks,n);return 1;}
     unsigned expected=(n>=TARGET_BASE-0x200000&&n<TARGET_BASE-0x200000+TABLE_SIZE)?table[n-(TARGET_BASE-0x200000)]:0xa5;
     if(sram[n]!=expected){fprintf(stderr,"SRAM mismatch case%u byte%x %x/%x\n",checks,n,sram[n],expected);return 1;}
    }
   }
  }
  if(cost>peak)peak=cost;cycles+=cost;checks++;
 }
 printf("Original690C rowscroll relocation: %u cases, all256 transition bytes x32 counter/CCR boundaries; %u negative initializations, %llu word writes confined exactly to922000..9227FF. Full work RAM, all65536 SRAM bytes/canaries, registers/CCR/SP/USP/SSP and exact cycles match. Mean/peak %.3f/%u mastercycles. Test-only20C800 mapping replaces displayed latch; simultaneous production allocation remains unadmitted.\n",checks,negative,totalWrites,(double)cycles/checks,peak);
 return 0;
}
