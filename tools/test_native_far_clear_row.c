/* Independent whole-memory oracle for the exact extracted native helper. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "clear-fixture.h"
#undef CHECK
static unsigned cases,reads,unexpectedVDP;
static uint64_t byteChecks,baselineCycles[5],candidateCycles[5],timingCases[5];
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clear row case%u line%u: %s\n",cases,__LINE__,#x);exit(1);}}while(0)
static uint8_t initialRAM[65536],expectedRAM[65536],savedSRAM[65536],savedVRAM[65536];
static uint8_t savedCRAM[128],savedVSRAM[128],savedRegs[32];
static unsigned rowStart,allowed[64],watchReads;
static unsigned readRAM8(unsigned at){return ram[(at&65535)^1];}
static unsigned readRAM16(unsigned at)
{
 if(watchReads&&at>=CLEAR_WORDS&&at<CLEAR_WORDS+4096){
  CHECK(at>=rowStart&&at<rowStart+128);CHECK(allowed[(at-rowStart)/2]);reads++;
 }
 return rw(at);
}
static void writeVDP(unsigned at,unsigned value){unexpectedVDP++;vdpWrite16(at,value);}
static void makeState(unsigned x,unsigned y,unsigned count,unsigned pattern)
{
 for(unsigned n=0;n<65536;n++)initialRAM[n]=(n*53+(n>>8)*17+0x35)&255;
 unsigned words=CLEAR_WORDS&65535;
 /* Every surrounding word is a nonzero canary. The addressed row has the
  * declared sparse/dense mixture, including realFFFF and flipped words. */
 for(unsigned col=0;col<64;col++){
  unsigned value=0;
  if(pattern==1)value=0xffff;
  if(pattern==2)value=(col%2)?0xfabc:0;
  if(pattern==3)value=(col==((x+count-1)&63))?0x8fff:0;
  if(pattern==4)value=(col==((x+count/2)&63))?0xffff:0;
  pw(initialRAM+words+(y&31)*128+col*2,value);
 }
 unsigned dirty=(pattern&1)?1u<<(y&31):0;
 pl(initialRAM+(CLEAR_DIRTY&65535),dirty);
 memset(initialRAM+(CLEAR_FIRST&65535),17,32);memset(initialRAM+(CLEAR_LAST&65535),42,32);
 pl(initialRAM+(CLEAR_COLUMNS&65535),0x20000002);pl(initialRAM+(CLEAR_COLUMNS&65535)+4,0x80000001);
 memcpy(expectedRAM,initialRAM,65536);memset(allowed,0,sizeof allowed);
 for(unsigned n=0;n<count;n++){
  unsigned col=(x+n)&63,where=words+(y&31)*128+col*2;allowed[col]=1;
  if(!word(expectedRAM+where))continue;
  pw(expectedRAM+where,0);
  unsigned d=lng(expectedRAM+(CLEAR_DIRTY&65535)),bit=1u<<(y&31);
  unsigned first=(CLEAR_FIRST&65535)+(y&31),last=(CLEAR_LAST&65535)+(y&31);
  if(!(d&bit)){expectedRAM[first]=expectedRAM[last]=col;pl(expectedRAM+(CLEAR_DIRTY&65535),d|bit);}
  else{if(col<expectedRAM[first])expectedRAM[first]=col;if(col>expectedRAM[last])expectedRAM[last]=col;}
  unsigned at=(CLEAR_COLUMNS&65535)+(col/32)*4;
  pl(expectedRAM+at,lng(expectedRAM+at)|(1u<<(col&31)));
 }
 rowStart=CLEAR_WORDS+(y&31)*128;
 memset(savedSRAM,0xa5,65536);pw(savedSRAM+0xf800,x);pw(savedSRAM+0xf802,y);pw(savedSRAM+0xf804,count);
}
static unsigned invoke(unsigned entry,unsigned seed)
{
 for(unsigned n=0;n<65536;n++)ram[n^1]=initialRAM[n];memcpy(sram,savedSRAM,65536);
 unsigned saved[15],stack=0xff7800;
 setreg(17,0x2700|(seed&31));setreg(15,stack-4);wl(stack-4,0x100000);
 for(unsigned n=0;n<15;n++){saved[n]=0x9a730000+n*0x1003+seed;setreg(n,saved[n]);}
 setreg(16,entry);cpu->cycles=0;cpu->refresh=896;unsigned steps=0;
 watchReads=1;
 while(getreg(16)!=0x100000&&steps++<200000)run68k(cpu->cycles+1);
 watchReads=0;
 CHECK(steps<200000&&getreg(15)==stack);
 for(unsigned n=2;n<15;n++)if(n!=8&&n!=9)CHECK(getreg(n)==saved[n]);
 for(unsigned n=0;n<65536;n++){
  if(n<0x7000||n>=0x7800)CHECK(ram[n^1]==expectedRAM[n]);
  CHECK(sram[n]==savedSRAM[n]);CHECK(vram[n]==savedVRAM[n]);
 }
 CHECK(!unexpectedVDP);byteChecks+=65536*3-2048;return cpu->cycles;
}
int main(int argc,char**argv)
{
 if(argc!=4)return 2;
 FILE*file=fopen(argv[2],"rb");CHECK(file);size_t bytes=fread(rom+0x102000,1,sizeof rom-0x102000,file);CHECK(bytes&&feof(file));fclose(file);
 pl(rom,0xffff00);pl(rom+4,0x100004);pw(rom+0x100000,0x60fe);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x33,0xfc,0x81,4,0,0xc0,0,4,0x33,0xfc,0x8f,2,0,0xc0,0,4,0x4e,0xf9,0,0x10,0,0};memcpy(rom+0x100004,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];CHECK(snprintf(gamepath,sizeof gamepath,"%s.closed.bin",argv[2])<(int)sizeof gamepath);
 file=fopen(gamepath,"wb");CHECK(file&&fwrite(rom,1,sizeof rom,file)==sizeof rom);fclose(file);
 void*core=dlopen(argv[1],RTLD_NOW);CHECK(core);
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");CHECK(setreg&&getreg&&run68k);
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={gamepath,rom,sizeof rom,0};CHECK(retro_load_game(&game));
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");vram=dlsym(core,"vram");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);CHECK(ram&&cpu&&vram&&sram);
 retro_run();CHECK(getreg(16)==0x100000);cpu->map[0xff].read8=readRAM8;cpu->map[0xff].read16=readRAM16;
 vdpWrite16=cpu->map[0xc0].write16;CHECK(vdpWrite16);cpu->map[0xc0].write16=writeVDP;
 memcpy(savedVRAM,vram,65536);memcpy(savedCRAM,dlsym(core,"cram"),128);memcpy(savedVSRAM,dlsym(core,"vsram"),128);memcpy(savedRegs,dlsym(core,"reg"),32);
 file=fopen(argv[3],"w");CHECK(file);fputs("case,x,y,count,pattern,baseline_master,candidate_master\n",file);
 for(unsigned pattern=0;pattern<5;pattern++)for(unsigned x=0;x<64;x++)for(unsigned count=0;count<=64;count++){
  unsigned y=(x*7+count*13)&31,high=(count%3)?0:0xffc0;
  makeState(x|high,y|(high&0xffe0),count,pattern);
  unsigned old=invoke(BASELINE_ENTRY,cases),new=invoke(CLEAR_ENTRY,cases);
  CHECK(!memcmp(savedCRAM,dlsym(core,"cram"),128)&&!memcmp(savedVSRAM,dlsym(core,"vsram"),128)&&!memcmp(savedRegs,dlsym(core,"reg"),32));
  fprintf(file,"%u,%u,%u,%u,%u,%u,%u\n",cases,x|high,y|(high&0xffe0),count,pattern,old,new);
  baselineCycles[pattern]+=old;candidateCycles[pattern]+=new;timingCases[pattern]++;cases++;
 }
 fclose(file);
 printf("{\"paired_cases\":%u,\"actual_68000_calls\":%u,\"byte_checks\":%llu,\"bounded_plane_word_reads\":%u,\"no_vdp_writes\":true,\"patterns\":[",cases,cases*2,(unsigned long long)byteChecks,reads);
 for(unsigned n=0;n<5;n++)printf("%s{\"pattern\":%u,\"cases\":%llu,\"baseline_master_sum\":%llu,\"candidate_master_sum\":%llu}",n?",":"",n,(unsigned long long)timingCases[n],(unsigned long long)baselineCycles[n],(unsigned long long)candidateCycles[n]);
 printf("]}\n");return 0;
}
