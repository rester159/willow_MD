/* Closed ABI calls into an unchanged compiled renderer. Host seeds between
 * calls are diagnostic only; no GUI, scheduler, or production timing claim. */
#include "vendor/libretro.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#ifndef WILLOW_CLOSED_HELPERS_ONLY
#include "closed-fixture.h"
#endif
typedef struct {
 struct {void*base;unsigned(*read8)(unsigned);unsigned(*read16)(unsigned);void(*write8)(unsigned,unsigned);void(*write16)(unsigned,unsigned);} map[256];
 struct {unsigned pc,cycle,detected;} poll;
 int cycles,refresh;unsigned end,dar[16],pc;
} CPU;
#ifndef WILLOW_TEST_ROM_BYTES
#define WILLOW_TEST_ROM_BYTES 0x400000
#endif
static uint8_t rom[WILLOW_TEST_ROM_BYTES],*ram,*sram,*vram;
static CPU*cpu;
static void(*setreg)(int,unsigned);static unsigned(*getreg)(int);static void(*run68k)(unsigned);
static void(*vdpWrite16)(unsigned,unsigned);static unsigned vdpDataWords,vdpControlWords;
static void observeVDP(unsigned at,unsigned value){
 if((at&0xfffffcu)==0xc00000)vdpDataWords++;
 if((at&0xfffffcu)==0xc00004)vdpControlWords++;
 vdpWrite16(at,value);
}
static bool env(unsigned cmd,void*data){
 if(cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY||cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY){*(const char**)data="/tmp";return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE){*(bool*)data=false;return true;}
 if(cmd==RETRO_ENVIRONMENT_GET_VARIABLE){((struct retro_variable*)data)->value=NULL;return false;}
 if(cmd==RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION){*(unsigned*)data=0;return true;}
 return cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT||cmd==RETRO_ENVIRONMENT_SET_VARIABLES||cmd==RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS||cmd==RETRO_ENVIRONMENT_SET_CONTROLLER_INFO;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t a,int16_t b){(void)a;(void)b;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned a,unsigned b,unsigned c,unsigned d){(void)a;(void)b;(void)c;(void)d;return 0;}
static unsigned word(const uint8_t*p){return p[0]*256+p[1];}
static unsigned lng(const uint8_t*p){return word(p)*65536u+word(p+2);}
static void pw(uint8_t*p,unsigned v){p[0]=v>>8;p[1]=v;}
static void pl(uint8_t*p,unsigned v){pw(p,v>>16);pw(p+2,v);}
static unsigned rw(unsigned a){return ram[(a&65535)^1]*256+ram[((a+1)&65535)^1];}
static unsigned rl(unsigned a){return rw(a)*65536u+rw(a+2);}
static void ww(unsigned a,unsigned v){ram[(a&65535)^1]=v>>8;ram[((a+1)&65535)^1]=v;}
static void wl(unsigned a,unsigned v){ww(a,v>>16);ww(a+2,v);}
#ifndef WILLOW_CLOSED_MAX_STEPS
#define WILLOW_CLOSED_MAX_STEPS 200000
#endif
static unsigned call(unsigned pc,unsigned stack,unsigned seed){
 unsigned saved[15];
 setreg(17,0x2700|(seed&31));setreg(15,stack-4);wl(stack-4,0x100000);
 for(unsigned n=0;n<15;n++){saved[n]=0x13790000u+n*0x1021u+seed;setreg(n,saved[n]);}
 setreg(16,pc);cpu->cycles=0;cpu->refresh=896;
 unsigned steps=0;
 while(getreg(16)!=0x100000&&steps++<WILLOW_CLOSED_MAX_STEPS)run68k(cpu->cycles+1);
 if(steps>=WILLOW_CLOSED_MAX_STEPS||getreg(15)!=stack){fprintf(stderr,"Call did not return pc%x sp%x\n",getreg(16),getreg(15));exit(3);}
 for(unsigned n=2;n<15;n++)if(n!=8&&n!=9&&getreg(n)!=saved[n]){fprintf(stderr,"C ABI register%u changed %x/%x\n",n,getreg(n),saved[n]);exit(3);}
 return (unsigned)cpu->cycles;
}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"Closed check line%u step%u %s phase%x\n",__LINE__,i,t->name,word(sram+0xf540));return 1;}}while(0)
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n)return 2
#ifndef WILLOW_CLOSED_HELPERS_ONLY
int main(int argc,char**argv){
 if(argc!=4)return 2;
 FILE*f=fopen(argv[2],"rb");if(!f)return 2;
 size_t bytes=fread(rom+0x102000,1,sizeof rom-0x102000,f);if(!bytes||!feof(f))return 2;fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x100000);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x100000,boot,sizeof boot);
 const uint8_t displayOff[]={0x33,0xfc,0x81,0x04,0,0xc0,0,4,0x4e,0x75};memcpy(rom+0x100010,displayOff,sizeof displayOff);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];if(snprintf(gamepath,sizeof gamepath,"%s.closed.bin",argv[2])>=(int)sizeof gamepath)return 2;
 f=fopen(gamepath,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core){fprintf(stderr,"Core load: %s\n",dlerror());return 2;}
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={gamepath,rom,sizeof rom,0};if(!retro_load_game(&game)){fprintf(stderr,"Game load failed\n");return 2;}
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");vram=dlsym(core,"vram");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!vram||!sram){fprintf(stderr,"Missing core memory %p/%p/%p/%p\n",(void*)ram,(void*)cpu,(void*)vram,(void*)sram);return 2;}
 retro_run();call(BOOT,0xff7800,0);call(0x100010,0xff7800,0);
 vdpWrite16=cpu->map[0xc0].write16;if(!vdpWrite16)return 2;cpu->map[0xc0].write16=observeVDP;
 f=fopen(argv[3],"w");if(!f)return 2;
 fputs("step,name,prepare_phase,publish_phase,reuse,build,prepare_master,publish_master,pieces,geometry,data_words,control_words\n",f);
 unsigned successes=0,refusals=0,reuses=0,builds=0,groups=0;
 unsigned publishedGeometry=0,publishedPalette=0,publishedPieces=0,zeroTransfers=0,totalData=0;
 for(unsigned i=0;i<sizeof tests/sizeof *tests;i++){
  const Test*t=tests+i;
  if(t->reset){memset(ram,0xa5,65536);for(unsigned n=0x4000;n<0x5000;n++)ram[n^1]=0;memset(sram,0,65536);groups++;publishedGeometry=publishedPalette=publishedPieces=0;}
  ww(0xff8004,2);ww(0xff8008,2);
  unsigned base=t->bank?0xd000:0xc000;
  memset(sram+base,0,2048);for(unsigned n=0;n<t->count*4;n++)pw(sram+base+n*2,t->records[n]);
  memcpy(sram+0xd800,sourcePalette,6144);if(t->badPalette)sram[0xd802]^=1;
  pw(sram+0xf504,t->bank*8);pl(sram+0xf508+t->bank*8,t->epoch);pw(sram+0xf50c+t->bank*8,t->count);pw(sram+0xf50e + t->bank*8,t->valid);
  pl(sram+0xf568,t->paletteEpoch);pw(sram+0xf56e,t->busy);if(t->reset||t->invalidate)pw(sram+0xf56c,0);
  pw(sram+0xf90a,t->badView==1?0:0x91c0);pw(sram+0xf922,t->badView==2?0x803e:0x3e);
  uint8_t sourceBefore[0x3000],vramBefore[640];memcpy(sourceBefore,sram+0xc000,sizeof sourceBefore);memcpy(vramBefore,vram+0xf400,640);
  unsigned r0=lng(sram+0xf584),b0=lng(sram+0xf588);
  unsigned prep=call(PREPARE,0xff7600,i),phase=word(sram+0xf540),cache=CACHES+t->bank*CACHE_SIZE;
  CHECK(memcmp(sourceBefore,sram+0xc000,sizeof sourceBefore)==0);
  CHECK((t->accept&&phase==2)||(!t->accept&&(phase&255)==5));
  if(t->status)CHECK(phase==(t->status*256+5));
  unsigned reused=lng(sram+0xf584)-r0,built=lng(sram+0xf588)-b0;
  CHECK(reused==t->reuse);CHECK(built==(t->accept&&!t->reuse));
  if(t->accept){
   CHECK(rw(cache+READY)==1&&rw(cache+PIECES)==t->pieces&&rw(cache+RECORDS)==t->count);
   CHECK(rl(cache+SOURCE_EPOCH)==t->epoch&&rl(cache+PALETTE_EPOCH)==t->paletteEpoch);
#if CONTENT_STREAM
   CHECK(rl(cache+GEOMETRY_ID)==t->geometry);
#endif
   for(unsigned n=0;n<t->count*4;n++)CHECK(rw(cache+SOURCE_WORDS+n*2)==t->records[n]);
   for(unsigned n=0;n<t->pieces*4;n++)CHECK(rw(cache+n*2)==t->sat[n]);
   if(!reused)for(unsigned n=0;n<224;n++){CHECK(ram[((BUDGET+BUDGET_SPRITES+n)&65535)^1]==t->sprites[n]);CHECK(rw(BUDGET+BUDGET_PIXELS+n*2)==t->sprites[n]*16);}
   successes++;
  }else{CHECK(rw(cache+READY)==0);refusals++;}
  pw(sram+0xf51e,t->bank);pw(sram+0xf530,t->count);pl(sram+0xf534,t->epoch);pw(sram+0xf532,1);
  if(t->post==1)pl(sram+0xf568,t->paletteEpoch+1);
  if(t->post==2)pw(sram+0xf922,0x803e);
  if(t->post==3)pw(sram+0xf56e,1);
  if(t->post==4)pw(sram+0xf532,0);
  if(t->post==5)pw(sram+0xf530,t->count+1);
  vdpDataWords=vdpControlWords=0;
  unsigned pub=call(PUBLISH,0xff7800,i+7),published=word(sram+0xf540);
  if(t->accept&&!t->post){
   CHECK(published==4);unsigned nwords=t->pieces?t->pieces*4:4;
   for(unsigned n=0;n<nwords;n++){unsigned expected=t->pieces?t->sat[n]:0,at=0xf400+n*2;CHECK((vram[at^1]*256+vram[(at+1)^1])==expected);}
#if CONTENT_STREAM
   unsigned skip=publishedGeometry==t->geometry&&publishedPalette==t->paletteEpoch&&publishedPieces==t->pieces;
   CHECK(vdpDataWords==(skip?0:nwords));CHECK(vdpControlWords==(skip?0:2));zeroTransfers+=skip;
   publishedGeometry=t->geometry;publishedPalette=t->paletteEpoch;publishedPieces=t->pieces;
#endif
  }else{CHECK((published&255)==5);CHECK(memcmp(vramBefore,vram+0xf400,640)==0);CHECK(vdpDataWords==0&&vdpControlWords==0);}
  for(unsigned n=0;n<65536;n++)if(!(n>=0x4000&&n<0x5000)&&!(n>=0x7000&&n<0x7800)&&!(n>=0x8004&&n<0x8006)&&!(n>=0x8008&&n<0x800a))CHECK(ram[n^1]==0xa5);
  reuses+=reused;builds+=built;
  totalData+=vdpDataWords;
  fprintf(f,"%u,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",i,t->name,phase,published,reused,built,prep,pub,t->pieces,t->geometry,vdpDataWords,vdpControlWords);
 }
 fclose(f);printf("PASS %u steps, %u groups, %u prepared, %u refused, %u reuse, %u build; exact SAT, cold-build budgets, cached source words, C ABI, source immutability, WRAM canaries, no VDP writes on refused publication. Content fast path: %u independently predicted zero-transfer publications, %u actual VDP data words.\n",(unsigned)(sizeof tests/sizeof *tests),groups,successes,refusals,reuses,builds,zeroTransfers,totalData);return 0;
}
#endif
