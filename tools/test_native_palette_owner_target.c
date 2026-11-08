#include "vendor/libretro.h"
#include "../src/willow_native_palette_mutable.h"
#include "../src/willow_arcade_palette_animation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifdef WILLOW_TEST_SPLIT_PALETTE
#define SLOT_COUNT 2
static unsigned slotAddress(unsigned n){return n?0x2300:0x2200;}
#else
#define SLOT_COUNT 8
static unsigned slotAddress(unsigned n){return 0x2200+n*34;}
#endif
static uint8_t rom[0x200000];
static uint16_t expected[3072];
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t readColor(uint32_t a){return expected[(a-0x91c000)/2];}
static void writeColor(uint32_t a,uint16_t v){expected[(a-0x91c000)/2]=v;}
static uint16_t ramword(const uint8_t*r,unsigned a){return ((uint16_t)r[a^1]<<8)|r[(a+1)^1];}
static void putram(uint8_t*r,unsigned a,uint16_t v){r[a^1]=v>>8;r[(a+1)^1]=v;}
static uint32_t rng=0x192480;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
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

/* Prefix layout from the local core header build/genplus-m68k.h. Validate
 * PC and all DAR fields through the exported register API before using it. */
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CoreCyclePrefix;
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/palette_owner_probe.bin","rb");if(!f||!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/palette_owner_probe.sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"paletteOwnerProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutWord(rom+0x100008,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/native-palette-owner-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/native-palette-owner-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Probe load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");CoreCyclePrefix*cpu=dlsym(core,"m68k");if(!ram||!cpu)return 2;
 m68k_set_reg(17,0x2700);m68k_set_reg(16,0x100008);
 if(cpu->pc!=0x100008)return 2;
 for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
 cpu->cycles=0;cpu->refresh=896;m68k_run(1);if(cpu->cycles!=28||m68k_get_reg(16)!=0x10000a)return 2;
 memset(ram,0xa5,65536);
 WillowPaletteAnimation g={.word=word,.read=readColor,.write=writeColor};
 #ifdef WILLOW_TEST_SPLIT_PALETTE
 const unsigned operations=5;
#else
 const unsigned operations=6;
#endif
 unsigned maxima[7]={0},counts[7]={0};
 for(unsigned step=0;step<1024;step++){
  unsigned command=step?1+(next()>>24)%operations:0,group=(next()>>16)%SLOT_COUNT;
  uint32_t destination=0x91c000+group*32,source=0xe0000+((next()>>12)%4096)*32;
  uint16_t value=next()>>8;
  if(command==1||command==5)destination+=((next()>>16)&15)*2;
  if(command==3)value=(value&1)?0x1000:0xf000;
  if(command==4)value%=7;
  if(command==6){
   const uint32_t sources[]={0xe9920,0xeb700,0xe72a0,0xe5280};
   const uint32_t targets[]={0x91c800,0x91cc00,0x91c400,0x91c200};
   unsigned block=(next()>>16)&3;source=sources[block]+(!block&&(step&1)?1024:0);
   destination=targets[block];value=block==3?256:512;
  }
  if(!command)for(unsigned n=0;n<3072;n++)expected[n]=(uint16_t)(word(0xe0000+n*2)+(((n/16)&15)<<12));
  else if(command==1)writeColor(destination,value);
  else if(command==2)willowPaletteCopy(source,destination,&g);
  else if(command==3)for(unsigned n=0;n<16;n++)expected[group*16+n]+=value;
  else if(command==4){uint16_t masks[4];if(!willowPaletteStageInit(value,masks,word,writeColor))return 2;}
  else if(command==6)for(unsigned n=0;n<value;n++)writeColor(destination+n*2,word(source+n*2));
  putram(ram,0x1000,command);putram(ram,0x1002,0);
  putram(ram,0x1004,source>>16);putram(ram,0x1006,source);
  putram(ram,0x1008,destination>>16);putram(ram,0x100a,destination);putram(ram,0x100c,value);
  putram(ram,0xff00,0x10);putram(ram,0xff02,0);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  cpu->cycles=0;cpu->refresh=896;
  unsigned instructions=0;
  while(m68k_get_reg(16)!=0x100000 && instructions++<200000)m68k_run(cpu->cycles+1);
  if(m68k_get_reg(16)!=0x100000||ramword(ram,0x1002)!=1){fprintf(stderr,"Target failed step %u op %u PC %x\n",step,command,m68k_get_reg(16));return 1;}
  if((unsigned)cpu->cycles>maxima[command])maxima[command]=cpu->cycles;counts[command]++;
  if(command==5 && ramword(ram,0x100e)!=readColor(destination))return 1;
  WillowNativePaletteOwner decoded;WillowMutablePaletteGroup slots[SLOT_COUNT];
  for(unsigned n=0;n<192;n++)decoded.sources.group[n]=ramword(ram,0x2000+n*2);
  for(unsigned n=0;n<96;n++)decoded.view.offsets[n]=ram[(0x2180+n)^1];
  for(unsigned n=0;n<SLOT_COUNT;n++){
   slots[n].group=ramword(ram,slotAddress(n));
   for(unsigned j=0;j<16;j++)slots[n].words[j]=ramword(ram,slotAddress(n)+2+j*2);
  }
  for(unsigned n=0;n<3072;n++){
   uint16_t actual;if(!willowMutablePaletteRead(&decoded,slots,SLOT_COUNT,0x91c000+n*2,word,&actual)||actual!=expected[n]){
    fprintf(stderr,"Target palette mismatch step %u op %u word %u\n",step,command,n);return 1;
   }
  }
  for(unsigned n=0;n<0xfc00;n++)if(!(n>=0x1000&&n<0x1010)&&!(n>=0x2000&&n<0x21e0)
#ifdef WILLOW_TEST_SPLIT_PALETTE
   &&!(n>=0x2200&&n<0x2222)&&!(n>=0x2300&&n<0x2322)
#else
   &&!(n>=0x2200&&n<0x2310)
#endif
   &&ram[n^1]!=0xa5){fprintf(stderr,"Target guard overwritten %x\n",n);return 1;}
 }
 for(unsigned n=0;n<=operations;n++)printf("Palette target op %u: %u calls, worst %.1f 68000 cycles (refresh included, VDP/DMA excluded)\n",n,counts[n],maxima[n]/7.0);
 puts("Native 68000 palette owner: 1024 mixed operations, 3145728 exact words and surrounding RAM guards match; slot count is fixture capacity, not a game memory budget");
 return 0;
}
