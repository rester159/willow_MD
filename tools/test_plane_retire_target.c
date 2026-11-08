/* Compiled span retirement and availability rebuild, independent byte model. */
#include "vendor/libretro.h"
#include "../src/willow_arcade_player_motion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
static uint8_t rom[0x200000];
static void put(uint8_t*r,unsigned at,unsigned value){r[at^1]=value>>8;r[(at+1)^1]=value;}
static unsigned get(const uint8_t*r,unsigned at){return (r[at^1]<<8)|r[(at+1)^1];}
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
 f=fopen("build/plane_retire_probe" ".bin","rb");if(!f)return 2;
 if(!fread(rom+0x100010,1,sizeof rom-0x100010,f))return 2;fclose(f);
 f=fopen("build/plane_retire_probe" ".sym","r");if(!f)return 2;
 unsigned entry=0,address;char line[512],kind,name[256];
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&address,&kind,name)==3&&!strcmp(name,"planeRetireProbe"))entry=address;
 fclose(f);if(!entry)return 2;
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 willowMotionPutWord(rom+0x100000,0x60fe);
 willowMotionPutWord(rom+0x100004,0x4e71);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 f=fopen("build/plane-retire-oracle.bin","wb");if(!f || fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 struct retro_game_info game={"build/plane-retire-oracle.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game)){fputs("Oracle ROM load failed\n",stderr);return 2;}
 uint8_t*ram=dlsym(core,"work_ram");if(!ram)return 2;


 uint8_t expected[0xfe00];unsigned calls=0;
 for(unsigned sample=0;sample<1024;sample++)for(unsigned mode=0;mode<2;mode++){
  for(unsigned n=0;n<65536;n++)ram[n]=(n*31+sample*17)^0xa5;
  for(unsigned n=0;n<1296;n++){
   unsigned mask=(sample*81+n/16)&65535;
   unsigned values[]={1,65535,32768,32767,256,2,65534,1u<<(n%16)};
   unsigned value=(mask&(1u<<(n%16)))?values[(sample+n)%8]:0;
   put(ram,0x3002+n*2,value);
  }
  unsigned stride=sample%3==0?43:sample%3==1?1:2;
  unsigned count=sample%7==0?0:stride==43?sample%29:sample%41;
  const unsigned bases[]={0,16,511,752};unsigned base=bases[sample%4];
  for(unsigned n=0;n<1376;n++)put(ram,0x4002+n*2,base+((sample*71+n*17)%1296)|(n&31)<<11);
  if(mode)for(unsigned n=0;n<count;n++){
   /* Seed repeated destinations with enough references; high count cases
    * still retain unrelated owners after this span retires. */
   unsigned tile=(sample%2)?(sample*71+n*17)%1296:sample%1296;
   put(ram,0x4002+n*stride*2,base+tile|((sample+n)&31)<<11);
   put(ram,0x3002+tile*2,(sample&4)?0x8000+count:(sample%2)?1:count);
  }
  const unsigned params[]={mode,base,count,stride,0x7777};
  for(unsigned n=0;n<5;n++)put(ram,0x1000+n*2,params[n]);
  memcpy(expected,ram,sizeof expected);
  if(mode)for(unsigned n=0;n<count;n++){
   unsigned tile=(get(expected,0x4002+n*stride*2)&2047)-base;
   unsigned at=0x3002+tile*2;put(expected,at,get(expected,at)-1);
  }
  for(unsigned g=0;g<6;g++)put(expected,0x2002+162+g*2,0);
  for(unsigned w=0;w<81;w++){
   unsigned bits=0;for(unsigned bit=0;bit<16;bit++)if(!get(expected,0x3002+(w*16+bit)*2))bits|=1u<<bit;
   put(expected,0x2002+w*2,bits);
   if(bits){unsigned at=0x2002+162+(w/16)*2;put(expected,at,get(expected,at)|(1u<<(w%16)));}
  }
  put(expected,0x1008,0x5242);
  put(ram,0xff00,0x10);put(ram,0xff02,0);
  for(unsigned r=2;r<=14;r++)if(r!=8&&r!=9)m68k_set_reg(r,0x12340000+sample*31+r*997);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,entry);
  unsigned frames=0;do{retro_run();frames++;}while(m68k_get_reg(16)!=0x100000&&frames<8);
  if(m68k_get_reg(16)!=0x100000||memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Plane retirement mismatch sample %u mode %u PC %x\n",sample,mode,m68k_get_reg(16));return 1;
  }
  for(unsigned r=2;r<=14;r++)if(r!=8&&r!=9&&m68k_get_reg(r)!=0x12340000+sample*31+r*997)return 1;
  calls++;
 }
 printf("68000 bulk retirement: %u calls; every 16-bit occupancy mask across 81-word rebuilds, high-bit counts, empty/contiguous/strided/repeated-tile spans, unchanged attributes/cursor, complete non-stack RAM and ABI registers match\n",calls);
 return 0;
}
