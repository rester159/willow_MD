/* Headless real-core DMA test of every extended background pattern. */
#include "vendor/libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include "../native/intro_probe/src/background_pairs.h"
static bool env(unsigned cmd,void*p){
 if(cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)return true;
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)p="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){struct retro_variable*v=p;if(!strcmp(v->key,"genesis_plus_gx_addr_error")){v->value="enabled";return true;}return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)p=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)p=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_VARIABLES || cmd==RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME || cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS || cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*p,unsigned w,unsigned h,size_t pitch){(void)p;(void)w;(void)h;(void)pitch;}
static void audio(int16_t a,int16_t b){(void)a;(void)b;}
static size_t batch(const int16_t*p,size_t n){(void)p;return n;}
static void poll(void){}
static int16_t input(unsigned a,unsigned b,unsigned c,unsigned d){(void)a;(void)b;(void)c;(void)d;return 0;}
static unsigned symbol(const char*path,const char*name){
 FILE*f=fopen(path,"r");assert(f);char line[512],sym[256],type;unsigned at;
 while(fgets(line,sizeof line,f))if(sscanf(line,"%x %c %255s",&at,&type,sym)==3 && !strcmp(sym,name)){fclose(f);return at&65535;}
 fprintf(stderr,"Missing symbol %s\n",name);exit(2);
}
static uint16_t word(const uint8_t*p,unsigned at){return p[at]|p[at+1]<<8;}
static void put(uint8_t*p,unsigned at,uint16_t v){p[at]=v;p[at+1]=v>>8;}
static uint8_t*readFile(const char*path,size_t*size){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);uint8_t*p=malloc(*size);assert(p&&fread(p,1,*size,f)==*size);fclose(f);return p;}
#define FN(ret,name,args) ret(*name)args=dlsym(core,#name);assert(name)
int main(int argc,char**argv){
 if(argc!=4)return 2;
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 size_t romSize,bankSize;uint8_t*rom=readFile(argv[2],&romSize),*bank=readFile("native/intro_probe/res/background_pairs.bin",&bankSize);
 assert(bankSize==(NATIVE_BACKGROUND_ID_LIMIT-NATIVE_BACKGROUND_EXTENSION_BASE)*32);
 struct retro_game_info game={argv[2],rom,romSize,NULL};assert(retro_load_game(&game));
 uint8_t*ram=dlsym(core,"work_ram"),*vram=dlsym(core,"vram");assert(ram&&vram);
 unsigned tick=symbol(argv[3],"debugIntroTick"),request=symbol(argv[3],"debugBankRequest"),first=symbol(argv[3],"debugBankFirst"),count=symbol(argv[3],"debugBankCount"),tiles=symbol(argv[3],"debugBankTiles"),overflow=symbol(argv[3],"debugIntroOverflow"),ids=symbol(argv[3],"ids"),refs=symbol(argv[3],"refsByTile");
 for(unsigned f=0;f<1400 && word(ram,tick)<915;f++)retro_run();assert(word(ram,tick)>=915 && !word(ram,overflow));
 uint8_t savedVRAM[65536];memcpy(savedVRAM,vram,sizeof savedVRAM);
 uint16_t protectedIds[1296],protectedRefs[1296]={0};memset(protectedIds,255,sizeof protectedIds);
 for(unsigned plane=0;plane<2;plane++)for(unsigned row=0;row<28;row++)for(unsigned col=0;col<41;col++){
  unsigned tile=word(vram,(plane?0xe000:0xc000)+(row*64+col)*2)&2047;
  assert(tile>=16 && tile<1312);protectedIds[tile-16]=word(ram,ids+(tile-16)*2);protectedRefs[tile-16]++;
 }
 unsigned checked=0;
 for(unsigned id=NATIVE_BACKGROUND_EXTENSION_BASE;id<NATIVE_BACKGROUND_ID_LIMIT;id+=4){
  put(ram,request,id+1);
  for(unsigned frame=0;frame<2;frame++){unsigned old=word(ram,tick);retro_run();if(word(ram,tick)!=old+1){fprintf(stderr,"Missed update loading background ID %u tick %u\n",id,old);return 1;}}
  assert(!word(ram,request) && !word(ram,overflow) && word(ram,first)==id);
  for(unsigned slot=0;slot<1296;slot++)if(protectedIds[slot]!=65535){
   assert(word(ram,ids+slot*2)==protectedIds[slot]);
   assert(!memcmp(vram+(16+slot)*32,savedVRAM+(16+slot)*32,32));
  }
  for(unsigned plane=0;plane<2;plane++)for(unsigned row=0;row<28;row++){
   unsigned at=(plane?0xe000:0xc000)+row*128;assert(!memcmp(vram+at,savedVRAM+at,82));
  }
  unsigned expected=NATIVE_BACKGROUND_ID_LIMIT-id;if(expected>4)expected=4;assert(word(ram,count)==expected);
  for(unsigned n=0;n<expected;n++){
   unsigned slot=word(ram,tiles+n*2);assert(slot<1296 && word(ram,ids+slot*2)==id+n && word(ram,refs+(16+slot)*2)==1+(protectedIds[slot]==id+n?protectedRefs[slot]:0));
   for(unsigned b=0;b<32;b++)if(vram[((16+slot)*32+b)^1]!=bank[(id+n-NATIVE_BACKGROUND_EXTENSION_BASE)*32+b]){
    fprintf(stderr,"Background DMA mismatch id %u slot %u byte %u got %02x expected %02x\n",id+n,slot,b,vram[((16+slot)*32+b)^1],bank[(id+n-NATIVE_BACKGROUND_EXTENSION_BASE)*32+b]);return 1;
   }
   checked++;
  }
 }
 printf("Native background bank: %u patterns / %u VRAM bytes checked, both visible planes preserved, no missed updates or cache overflows\n",checked,checked*32);
 free(bank);free(rom);return 0;
}
