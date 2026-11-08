/* Verify relocated original visibility instructions, C ABI and guarded binding. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include "../src/willow_arcade_visibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000],initial[65536],expected[65536],original[65536];
static uint16_t rw(uint16_t at){return willowMotionWord(expected+at);}
static uint8_t rb(uint16_t at){return expected[at];}
static void ww(uint16_t at,uint16_t v){willowMotionPutWord(expected+at,v);}
static void wb(uint16_t at,uint8_t v){expected[at]=v;}
static const WillowVisibilityBus bus={rw,rb,ww,wb};
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
static unsigned symbol(const char*path,const char*wanted){
 FILE*f=fopen(path,"r");if(!f)return 0;
 unsigned at,result=0;char kind,name[128];
 while(fscanf(f,"%x %c %127s",&at,&kind,name)==3)if(!strcmp(name,wanted))result=at;
 fclose(f);return result;
}
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} NativeVisibilityCycles;
static unsigned nonStack(unsigned at){return at<0xfec0||at>=0xff14;}
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;
 const char*base="build/native-instruction-audit/native_visibility_probe";
 char file[256];snprintf(file,sizeof file,"%s.sym",base);
 unsigned probe=symbol(file,"nativeVisibilityProbe"),leaf=symbol(file,"willowNativeVisibilityCode"),invoke=symbol(file,"willowNativeVisibilityInvoke");
 if(probe<0x100010||leaf<0x100010)return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;fclose(f);
 snprintf(file,sizeof file,"%s.bin",base);f=fopen(file,"rb");if(!f)return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;
 size_t bytes=fread(rom+0x100010,1,0xffff0,f);if(!bytes||!feof(f))return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;fclose(f);
 if(memcmp(rom+0x2244,rom+leaf,0x2508-0x2244)){fputs("Relocated leaf bytes differ from original\n",stderr);return 1;}
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);willowMotionPutWord(rom+0x100000,0x60fe);willowMotionPutWord(rom+0x10000e,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/native-instruction-audit/visibility-oracle.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/native-instruction-audit/visibility-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return fprintf(stderr,"Setup failure at line %u\n",__LINE__),2;
 FN(void,m68k_run,(unsigned));NativeVisibilityCycles*cpu=dlsym(core,"m68k");if(!cpu)return 2;
 unsigned cycleMode=argc==3;FILE*timing=cycleMode?fopen("build/native-instruction-audit/visibility-cycles.csv","w"):NULL;
 if(cycleMode){
  if(!timing||strcmp(argv[2],"--cycles"))return 2;
  m68k_set_reg(16,0x10000e);if(cpu->pc!=0x10000e)return 2;
  for(unsigned n=0;n<16;n++)if(cpu->dar[n]!=m68k_get_reg(n))return 2;
  cpu->cycles=0;cpu->refresh=896;m68k_run(1);
  if(cpu->cycles!=28||m68k_get_reg(16)!=0x100010)return 2;
  fputs("mode,class,lane,sample,active,visible,original_cycles,relocated_cycles,invoke_cycles,guarded_probe_cycles\n",timing);
 }
 unsigned registers[16];
#define RUN(entry,isSource) do { \
 for(unsigned a=0;a<65536;a++)ram[a^1]=initial[a]; \
 ram[0xff00^1]=0x00;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0; \
 m68k_set_reg(17,0x2715); \
 for(unsigned n=0;n<15;n++){registers[n]=0x53170000+n*0x1111;m68k_set_reg(n,registers[n]);} \
 if(isSource){registers[13]=0xff8000;registers[14]=0xff0000+handle;m68k_set_reg(13,registers[13]);m68k_set_reg(14,registers[14]);} \
 m68k_set_reg(15,0xffff00);m68k_set_reg(16,(entry)); \
 if(cycleMode){cpu->cycles=0;cpu->refresh=896;unsigned count=0;while(m68k_get_reg(16)!=0x100000&&count++<20000)m68k_run(cpu->cycles+1);} \
 else for(unsigned slice=0;slice<4&&m68k_get_reg(16)!=0x100000;slice++)retro_run(); \
 if(m68k_get_reg(16)!=0x100000){fprintf(stderr,"No return at entry %x pc %x\n",(entry),m68k_get_reg(16));return 1;} \
 if(m68k_get_reg(15)!=0xffff04)return 1; \
 for(unsigned n=2;n<15;n++)if(n!=8&&n!=9&&m68k_get_reg(n)!=registers[n]){fprintf(stderr,"ABI register %u changed entry %x\n",n,(entry));return 1;} \
}while(0)
 const unsigned entries[]={0x2244,0x2296,0x22dc};
 const unsigned pointers[]={0x9818,0x981a,0x981e,0x9820,0xf898,0x9824,0x9826,
  0x982a,0x982c,0x9830,0x9832,0x9836,0x9838,0xf900,0x983c,0x983e,0x9842,0x9844,0xf488,0xf48a};
 unsigned checks=0;
 for(unsigned mode=0;mode<3;mode++)for(unsigned selector=0;selector<=18;selector+=2)
 for(unsigned lane=0;lane<((selector==4||selector==12)?3:2);lane++)for(unsigned sample=0;sample<256;sample++){
  for(unsigned a=0;a<65536;a++)expected[a]=(a*73+sample*11)>>2;
  for(unsigned j=0;j<sizeof pointers/sizeof*pointers;j++)ww(pointers[j],0xd000+j*64+(sample%8)*2);
  uint16_t handle=0xa000+(sample%32)*64;uint8_t*r=expected+handle;
  r[0]=sample%5;r[1]=0xa7;r[30]=selector;r[14]=lane*2;
  uint16_t cx=sample*137,cy=sample*71;
  const unsigned xb[]={32,64,256},xl[]={448,512,896},yb[]={0,48,192},yl[]={272,368,640};
  const uint16_t xe[]={-xb[mode]-1,-xb[mode],0,xl[mode]-xb[mode],xl[mode]-xb[mode]+1,65535,32768,1};
  const uint16_t ye[]={-yb[mode]-1,-yb[mode],0,yl[mode]-yb[mode],yl[mode]-yb[mode]+1,65535,32768,1};
  ww(handle+6,cx+(sample<64?sample-32:xe[sample%8]));ww(handle+10,cy+(sample<64?sample:ye[(sample/8)%8]));ww(0x8408,cx);ww(0x840c,cy);
  willowMotionPutLong(expected+0x1000,0xff0000+handle);ww(0x1004,handle);ww(0x1006,cx);ww(0x1008,cy);wb(0x100a,mode);wb(0x100c,0xa5);
  memcpy(initial,expected,65536);
  willowArcadeVisibilityQueue(r,handle,cx,cy,mode,&bus);
  RUN(entries[mode],1);unsigned sourceCycles=cpu->cycles;
  unsigned originalRegs[18];for(unsigned n=0;n<18;n++)originalRegs[n]=m68k_get_reg(n);
  for(unsigned a=0;a<65536;a++){original[a]=ram[a^1];if(nonStack(a)&&ram[a^1]!=expected[a]){fprintf(stderr,"Host parity case %u mode%u class%u lane%u byte%x\n",checks,mode,selector,lane,a);return 1;}}
  RUN(leaf+entries[mode]-0x2244,1);unsigned rawCycles=cpu->cycles;
  for(unsigned a=0;a<65536;a++)if(nonStack(a)&&ram[a^1]!=original[a]){fprintf(stderr,"Raw relocated mismatch case%u byte%x\n",checks,a);return 1;}
  for(unsigned n=0;n<18;n++)if(n!=16){
   unsigned got=m68k_get_reg(n);if(n==8&&got>=leaf&&got<leaf+708)got=got-leaf+0x2244;
   if(got!=originalRegs[n]){fprintf(stderr,"Raw register mismatch case%u reg%u: %x/%x\n",checks,n,got,originalRegs[n]);return 1;}
  }
  willowMotionPutLong(initial+0xff04,0xff0000+handle);willowMotionPutLong(initial+0xff08,mode);
  RUN(invoke,0);unsigned invokeCycles=cpu->cycles;
  for(unsigned a=0;a<65536;a++)if(nonStack(a)&&ram[a^1]!=expected[a]){fprintf(stderr,"Raw C ABI mismatch case%u byte%x\n",checks,a);return 1;}
  RUN(probe,0);unsigned guardCycles=cpu->cycles;expected[0x100c]=1;
  if(timing)fprintf(timing,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",mode,selector,lane,sample,r[0],r[1],sourceCycles,rawCycles,invokeCycles,guardCycles);
  for(unsigned a=0;a<65536;a++)if(nonStack(a)&&ram[a^1]!=expected[a]){fprintf(stderr,"Guarded C ABI mismatch case%u byte%x: %x/%x\n",checks,a,ram[a^1],expected[a]);return 1;}
  checks++;
 }
 unsigned rejects=0;
 for(unsigned kind=0;kind<14;kind++){
  memset(initial,0xa5,65536);uint16_t handle=0xa000;uint8_t*r=initial+handle;
  r[0]=1;r[30]=2;r[14]=0;
  willowMotionPutWord(initial+0x9818,0xd000);willowMotionPutWord(initial+0x8408,0x1234);willowMotionPutWord(initial+0x840c,0x5678);
  willowMotionPutLong(initial+0x1000,0xffa000);willowMotionPutWord(initial+0x1004,handle);
  willowMotionPutWord(initial+0x1006,0x1234);willowMotionPutWord(initial+0x1008,0x5678);initial[0x100a]=0;
  switch(kind){
  case 0:initial[0x100a]=3;break;
  case 1:willowMotionPutWord(initial+0x1004,handle+1);willowMotionPutLong(initial+0x1000,0xffa001);break;
  case 2:willowMotionPutWord(initial+0x1004,0xffe2);willowMotionPutLong(initial+0x1000,0xffffe2);break;
  case 3:willowMotionPutLong(initial+0x1000,0x200000);break;
  case 4:willowMotionPutWord(initial+0x1006,0x1235);break;
  case 5:willowMotionPutWord(initial+0x1008,0x5679);break;
  case 6:r[30]=1;break;
  case 7:r[30]=20;break;
  case 8:r[14]=1;break;
  case 9:r[14]=4;break;
  case 10:r[30]=4;r[14]=6;break;
  case 11:willowMotionPutWord(initial+0x9818,0xd001);break;
  case 12:willowMotionPutWord(initial+0x9818,0x7ffe);break;
  case 13:willowMotionPutWord(initial+0x9818,0x8000);break;
  }
  memcpy(expected,initial,65536);expected[0x100c]=0;RUN(probe,0);
  for(unsigned a=0;a<65536;a++)if(nonStack(a)&&ram[a^1]!=expected[a]){fprintf(stderr,"Rejected guard %u mutated %x\n",kind,a);return 1;}
  rejects++;
 }
 if(timing)fclose(timing);
 printf("Native original-instruction visibility: %u cases, three viewports and all 20 queue lanes; original/host/relocated/C ABI RAM matches; raw registers/CCR and C callee-saved registers preserved; %u unsafe-input guards reject without mutation\n",checks,rejects);
 return 0;
}
