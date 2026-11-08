/* Compiled 68000 verification of the complete retained scene-clear sink. */
#include "vendor/libretro.h"
#include "scene_memory_probe_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static SceneMemoryProbeStorage model;
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

int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 f=fopen("build/scene_memory_probe" ".bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/scene_memory_probe" ".sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"sceneMemoryProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100004,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/scene-memory-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/scene-memory-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;

 uint8_t expected[0xfe00];
 _Static_assert(sizeof model==37540,"probe storage layout changed");
 unsigned accepted=0,rejected=0;
 for(unsigned sample=0;sample<1024;sample++){
  if(!(sample%32)){
   memset(ram,0xa5,65536);
   for(unsigned n=0;n<sizeof model/2;n++)((uint16_t*)&model)[n]=(sample*31+n*17)^0xa55a;
   for(unsigned n=0;n<4096;n++){
    model.nearMap[n]=(sample+n)%3;
    willowScroll3MapSet(model.farMap,n,(sample+n*2)%3);
   }
   for(unsigned n=0;n<8192;n++)willowHudSurfaceWrite(&model.hud,0x910000UL+n*2,model.text.words[n]);
   for(unsigned n=0;n<sizeof model/2;n++){
    uint16_t v=((uint16_t*)&model)[n];ram[(0x2002+n*2)^1]=v>>8;ram[(0x2003+n*2)^1]=v;
   }
  }
  unsigned op=0,at=0x910000,value=0x7020,second=0,count=4096,fault=0;
  switch(sample%32){
   case 0:break;
   case 1:at=0x918000;value=0;break;
   case 2:at=0x914000;value=0xa00;break;
   case 3:at=0x900000;value=0;count=512;break;
   case 4:at=0x908000;value=0;count=512;break;
   case 5:op=1;at=0x910000;value=sample;break;
   case 6:op=1;at=0x913ffe;value=sample;break;
   case 7:op=1;at=0x910008;value=sample;break;
   case 8:op=1;at=0x91000a;value=sample;break;
   case 9:op=1;at=0x910000+((sample*17)%8192)*2;value=sample;break;
   case 10:at++;break;
   case 11:at+=4;break;
   case 12:count--;break;
   case 13:second=1;break;
   case 14:value=0x7120;break;
   case 15:at=0x918000;break;
   case 16:at=0x914000;value=0;break;
   case 17:at=0x900000;value=0;count=511;break;
   case 18:at=0x908000;value=1;count=512;break;
   case 19:op=1;at=0x910001;break;
   case 20:op=1;at=0x90fffe;break;
   case 21:op=1;at=0x914000;break;
   case 22:op=1;at=0xffffffff;break;
   case 23:op=2;break;
   case 24:case 25:case 26:case 27:case 28:fault=sample%32-23;break;
   case 29:op=1;fault=1;break;
   case 30:op=1;fault=5;break;
   case 31:at=0x91c000;break;
  }
  uint16_t params[]={op,0x7777,at>>16,at,value,second,count,fault};
  for(unsigned n=0;n<8;n++){ram[(0x1000+n*2)^1]=params[n]>>8;ram[(0x1001+n*2)^1]=params[n];}
  memcpy(expected,ram,sizeof expected);
  unsigned ok=sceneMemoryProbeRun(&model,params);
  if(ok!=(sample%32<10)){fprintf(stderr,"Unexpected request status %u\n",sample);return 1;}
  if(ok)accepted++;else rejected++;
  unsigned status=ok?0x544b:1;
  expected[0x1002^1]=status>>8;expected[0x1003^1]=status;
  for(unsigned n=0;n<sizeof model/2;n++){
   uint16_t v=((uint16_t*)&model)[n];expected[(0x2002+n*2)^1]=v>>8;expected[(0x2003+n*2)^1]=v;
  }
  /* Rejection must preserve all storage, including HUD dirty bits. */
  if(!ok)for(unsigned n=0x2002;n<0x2002+sizeof model;n++)if(expected[n^1]!=ram[n^1])return 1;
  ram[0xff00^1]=0;ram[0xff01^1]=0x10;ram[0xff02^1]=0;ram[0xff03^1]=0;
  for(unsigned r=2;r<=14;r++)if(r!=8&&r!=9)m68k_set_reg(r,0x12340000+sample*31+r*997);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  unsigned slices=0;do{retro_run();slices++;}while(m68k_get_reg(16)!=0x100000&&slices<20);
  for(unsigned r=2;r<=14;r++)if(r!=8&&r!=9&&m68k_get_reg(r)!=0x12340000+sample*31+r*997)return 1;
  if(m68k_get_reg(16)!=0x100000||memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Target scene memory mismatch sample %u PC %x\n",sample,m68k_get_reg(16));return 1;
  }
 }
 printf("68000 complete retained scene memory: %u accepted fills/text writes, %u atomic rejections; all 37540 stored bytes, unrelated non-stack RAM and callee-saved registers match\n",accepted,rejected);
 return 0;
}
