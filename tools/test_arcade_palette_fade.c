/* Differential verification of shared services against original instructions. */
#include "vendor/libretro.h"
#include "../src/willow_native_palette_fade.h"
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
static unsigned ramWord(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
static uint16_t colors[3072];
static uint16_t colorRead(uint32_t at){if(at<0x91c000 || at>=0x91d800 || (at&1))abort();return colors[(at-0x91c000)/2];}
static void colorWrite(uint32_t at,uint16_t v){if(at<0x91c000 || at>=0x91d800 || (at&1))abort();colors[(at-0x91c000)/2]=v;}
static void colorAdjust(uint32_t address,uint16_t delta){
 for(unsigned n=0;n<16;n++)colorWrite(address+n*2,colorRead(address+n*2)+delta);
}
int main(int argc,char**argv){
 if(argc!=2&&(argc!=3||(strcmp(argv[2],"--grouped")&&strcmp(argv[2],"--owned"))))return 2;
 unsigned grouped=argc==3&&!strcmp(argv[2],"--grouped"),owned=argc==3&&!strcmp(argv[2],"--owned"),ownedChecks=0;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f || fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x100000]=0x60;rom[0x100001]=0xfe;
 /* Six palette base operands relocate; all fade instructions stay intact. */
 const unsigned patches[]={0x51ee,0x51fe,0x520e,0x521e,0x522e,0x523e};
 for(unsigned n=0;n<6;n++){
  uint32_t address=0x91c000+n*1024;
  if(willowMotionLong(rom+patches[n])!=address)return 2;
  willowMotionPutLong(rom+patches[n],0xff2000+n*1024);
 }
 /* Substitute only external scheduler services: yield at 1750 stops at
  * 100000; task exit at 165A stops at 100004. Resume a yield by its saved
  * return address. Fade code itself is unchanged; scheduler timing is not
  * measured by this instruction fixture. */
 if(memcmp(rom+0x1750,"\x48\xe7\xff\xfe\x4e\x43",6) ||
    memcmp(rom+0x165a,"\x4e\x49\x5c\x8f\x20\x6d",6))return 2;
 willowMotionPutWord(rom+0x1750,0x4ef9);willowMotionPutLong(rom+0x1752,0x100000);
 willowMotionPutWord(rom+0x165a,0x4ef9);willowMotionPutLong(rom+0x165c,0x100004);
 willowMotionPutWord(rom+0x100004,0x60fe);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/palette-fade-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/palette-fade-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

#define RUN(at,d0) do { \
 putRamWord(ram,0xff00,0x0010);putRamWord(ram,0xff02,0); \
 m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(15,0xffff00); \
 m68k_set_reg(0,(d0));m68k_set_reg(16,(at)); \
 for(unsigned slice=0;slice<4 && m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"Palette oracle stopped at PC %08x sample %u\n",m68k_get_reg(16),sample);return 1;} \
} while(0)
 unsigned checks=0;uint32_t random=0x57494c4c;
 const uint16_t deltas[]={0xf000,0x1000,0,0xffff,1,0x8000,0x7fff,0x1234};
 for(unsigned kind=0;kind<3;kind++)for(unsigned sample=0;sample<2048;sample++){
  WillowPaletteFade fade={0};fade.read=colorRead;fade.write=colorWrite;fade.adjustGroup=grouped?colorAdjust:NULL;
  for(unsigned n=0;n<6;n++){
   random=random*1664525u+1013904223u;fade.masks[n]=random;
   putRamWord(ram,0x82a8+n*4,random>>16);putRamWord(ram,0x82aa+n*4,random);
  }
  fade.sum=0xdeadbeef;putRamWord(ram,0xa3e,0xdead);putRamWord(ram,0xa40,0xbeef);
  for(unsigned n=0;n<3072;n++){
   random=random*1664525u+1013904223u;colors[n]=random>>16;putRamWord(ram,0x2000+n*2,colors[n]);
  }
  uint16_t delta=deltas[sample%8];unsigned bank=(sample/8)%6,group=(sample/48)%32,bit=(sample/3)&31;
  m68k_set_reg(2,delta);
  if(!kind && owned){
   WillowNativePaletteOwner owner={0},before;
   WillowMutablePaletteGroup slots[192],oldSlots[192];
   for(unsigned n=0;n<192;n++){
    owner.sources.group[n]=n<14?0xfff1+n:0xffff;
    willowNativePaletteOwnerOffset(&owner,n,n&15);slots[n].group=n;
    for(unsigned j=0;j<16;j++)slots[n].words[j]=colors[n*16+j]-((n&15)<<12);
   }
   before=owner;memcpy(oldSlots,slots,sizeof slots);WillowPaletteFade oldFade=fade;
   unsigned result=willowNativePaletteFadeAll(&owner,slots,192,&fade,delta,0);
   if(delta&0x0fff){
    if(result||memcmp(&owner,&before,sizeof owner)||memcmp(slots,oldSlots,sizeof slots)||memcmp(&fade,&oldFade,sizeof fade))return 1;
    willowPaletteFadeAll(&fade,delta);
   }else{
    if(!result)return 1;ownedChecks++;
    for(unsigned n=0;n<3072;n++)if(!willowMutablePaletteRead(&owner,slots,192,0x91c000+n*2,0,colors+n))return 1;
   }
  }else if(!kind)willowPaletteFadeAll(&fade,delta);
  else{
   unsigned at=0xff2000+bank*1024+(kind==1?group*32:0);
   m68k_set_reg(8,at);m68k_set_reg(9,0xff82a8+bank*4);m68k_set_reg(7,kind==1?bit:group);
   if(kind==1)willowPaletteFadeGroup(&fade,0x91c000+bank*1024+group*32,bank,bit,delta);
   else if(!willowPaletteFadeSelected(&fade,bank,group,delta))return 2;
  }
  RUN(!kind?0x51e8:kind==1?0x5260:0x5258,0xdeadbeef);
  for(unsigned n=0;n<6;n++){
   uint32_t actual=((uint32_t)ramWord(ram,0x82a8+n*4)<<16)|ramWord(ram,0x82aa+n*4);
   if(actual!=fade.masks[n]){fprintf(stderr,"Fade mask mismatch kind %u sample %u bank %u\n",kind,sample,n);return 1;}
  }
  uint32_t actual=((uint32_t)ramWord(ram,0xa3e)<<16)|ramWord(ram,0xa40);
  if(actual!=fade.sum){fprintf(stderr,"Fade sum mismatch kind %u sample %u\n",kind,sample);return 1;}
  for(unsigned n=0;n<3072;n++)if(colors[n]!=ramWord(ram,0x2000+n*2)){fprintf(stderr,"Fade color mismatch kind %u sample %u word %u native %04x source %04x\n",kind,sample,n,colors[n],ramWord(ram,0x2000+n*2));return 1;}
  checks++;
 }
 unsigned taskChecks=0,taskYields=0;
 for(unsigned sample=0;sample<256;sample++){
  WillowPaletteFade fade={0};fade.read=colorRead;fade.write=colorWrite;fade.adjustGroup=grouped?colorAdjust:NULL;
  WillowPaletteFadeTask task={0};task.fadeIn=sample&1;task.dark=(sample>>1)&1;
  task.busy=(sample>>2)&255;task.delay=sample;task.layerControl=sample*271;
  for(unsigned n=0;n<6;n++){
   fade.masks[n]=task.fadeIn?(n<4?0xffffffffUL:255):0;
   putRamWord(ram,0x82a8+n*4,fade.masks[n]>>16);putRamWord(ram,0x82aa+n*4,fade.masks[n]);
  }
  fade.sum=0xdeadbeef;putRamWord(ram,0xa3e,0xdead);putRamWord(ram,0xa40,0xbeef);
  for(unsigned n=0;n<3072;n++){
   random=random*1664525u+1013904223u;colors[n]=random>>16;putRamWord(ram,0x2000+n*2,colors[n]);
  }
  ram[0xf890^1]=task.busy;ram[0x9013^1]=task.delay;
  putRamWord(ram,0x82c0,task.dark);putRamWord(ram,0x804a,task.layerControl);
  m68k_set_reg(17,0x2700);m68k_set_reg(13,0xff8000);m68k_set_reg(14,0xff9000);m68k_set_reg(15,0xffff00);
  m68k_set_reg(16,task.fadeIn?0x5102:0x50c6);
  unsigned done=0;
  for(unsigned step=0;step<18;step++){
   uint8_t yield=willowPaletteFadeTaskStep(&task,&fade);
   for(unsigned slice=0;slice<4;slice++){
    retro_run();unsigned pc=m68k_get_reg(16);if(pc==0x100000 || pc==0x100004)break;
   }
   unsigned pc=m68k_get_reg(16);
   if(pc!=(yield?0x100000u:0x100004u) || (yield && m68k_get_reg(0)!=task.delay) ||
      ram[0xf890^1]!=task.busy || ramWord(ram,0x82c0)!=task.dark || ramWord(ram,0x804a)!=task.layerControl){fprintf(stderr,"Fade task mismatch sample %u step %u pc %x\n",sample,step,pc);return 1;}
   for(unsigned n=0;n<6;n++)if(fade.masks[n]!=(((uint32_t)ramWord(ram,0x82a8+n*4)<<16)|ramWord(ram,0x82aa+n*4)))return 1;
   if(fade.sum!=(((uint32_t)ramWord(ram,0xa3e)<<16)|ramWord(ram,0xa40)))return 1;
   for(unsigned n=0;n<3072;n++)if(colors[n]!=ramWord(ram,0x2000+n*2))return 1;
   taskChecks++;
   if(!yield){done=1;break;}
   taskYields++;
   unsigned sp=m68k_get_reg(15),at=sp&65535;
   unsigned resume=(ramWord(ram,at)<<16)|ramWord(ram,at+2);
   m68k_set_reg(15,sp+4);m68k_set_reg(16,resume);
  }
  if(!done){fprintf(stderr,"Fade task failed to finish sample %u\n",sample);return 1;}
 }
 printf("Fade tasks: %u yield/exit boundaries and %u requested delays match original fade-in/out code across 256 jobs\n",taskChecks,taskYields);
 printf("Palette fades: %u whole-bank, group and selected-group cases match all 3072 colors, six masks and completion sum\n",checks);
 if(owned)printf("Native whole-owner fade: %u original-instruction global comparisons; unsupported RGB deltas reject atomically\n",ownedChecks);
 return 0;
}
