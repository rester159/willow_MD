/* Closed native68000 post-source-clear context gate. No host writes during calls. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "far-boot-fixture.h"
#undef CHECK
#define MUST(v) do{if(!(v)){fprintf(stderr,"Far boot assertion line%d pc%x sp%x: %s\n",__LINE__,getreg(16),getreg(15),#v);exit(3);}}while(0)
static uint8_t reference[65536],referenceSram[65536];
static unsigned referenceRegs[21],nativeActive,writeCount,minStack,sourceWrites,executing;
static void(*oldRam8)(unsigned,unsigned),(*oldRam16)(unsigned,unsigned),(*oldSram8)(unsigned,unsigned),(*oldSram16)(unsigned,unsigned);
static void admitted(unsigned at,unsigned width){
 if(!nativeActive){if(at>=0xff0000&&at+width<=0x1000000)sourceWrites+=width;return;}
 MUST((at>=BSS_BEGIN&&at+width<=BSS_END)||(at>=0xff7000&&at+width<=0xff7600)||(at>=0x20f590&&at+width<=0x20f594));
 if(at>=0xff7000&&at<0xff7600&&at<minStack)minStack=at;
 writeCount+=width;
}
static void r8(unsigned a,unsigned v){admitted(a&0xffffff,1);if(oldRam8)oldRam8(a,v);else ram[(a&65535)^1]=v;}
static void r16(unsigned a,unsigned v){admitted(a&0xffffff,2);if(oldRam16)oldRam16(a,v);else ww(a,v);}
static void s8(unsigned a,unsigned v){admitted(a&0xffffff,1);if(oldSram8)oldSram8(a,v);else sram[a&65535]=v;}
static void s16(unsigned a,unsigned v){admitted(a&0xffffff,2);if(oldSram16)oldSram16(a,v);else pw(sram+(a&65535),v);}
static void load(const char*directory,const char*name,unsigned at){char p[4096];snprintf(p,sizeof p,"%s/%s",directory,name);FILE*f=fopen(p,"rb");if(!f)exit(2);size_t n=fread(rom+at,1,sizeof rom-at,f);if(!n||!feof(f))exit(2);fclose(f);}
static unsigned runCase(unsigned wrapper,unsigned direct,unsigned ccr,unsigned sp,unsigned seed){
 memset(ram,0xa5,65536);memset(sram,0x5a,65536);
 if(direct)memset(ram,0,65536);
 setreg(17,0x2700|ccr);setreg(15,sp);setreg(19,0xff1120);
 for(unsigned n=0;n<15;n++)setreg(n,0x71b30000u+seed*0x301+n*0x1101);
 setreg(12,wrapper?WRAPPER:0x5b6);setreg(16,direct?WRAPPER:0x6f2);
 nativeActive=0;writeCount=sourceWrites=0;minStack=0xff7600;cpu->cycles=0;cpu->refresh=896;
 unsigned steps=0;
 while(getreg(16)!=0x5b6&&steps++<2000000){executing=getreg(16);if(executing==WRAPPER)nativeActive=1;run68k(cpu->cycles+1);}
 MUST(steps<2000000);MUST(getreg(15)==sp&&getreg(19)==0xff1120);
 if(!direct)MUST(sourceWrites==65536);
 if(wrapper){
  MUST(minStack>=0xff7000&&minStack<0xff75be);MUST(rl(PREPARE_SP)==sp);MUST(rw(MISSING_REF)==65535);
  for(unsigned n=0;n<165;n++)MUST(rw(CACHE+n*2)==word(rom+POOL+n*4+2));
  MUST(rw(CACHE+400)==1); /* phase: target compiler fixture also asserts offset */
  MUST(rl(CACHE+402)==1);MUST(rw(CACHE+432)==0&&rw(CACHE+434)==0);
  MUST(word(sram+0xf590)==11&&word(sram+0xf592)==0);
 }
 return (unsigned)cpu->cycles;
}
int main(int argc,char**argv){
 if(argc!=4)return 2;load(argv[2],"program.bin",0);load(argv[2],"platform.bin",0x100010);load(argv[2],"background-resources.bin",0x110000);
 pl(rom,0xffff00);pl(rom+4,0x1f0000);const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x1f0000,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 char gamepath[4096];snprintf(gamepath,sizeof gamepath,"%s/closed-boot.bin",argv[2]);FILE*f=fopen(gamepath,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();struct retro_game_info game={gamepath,rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;retro_run();m68k_set_irq(0);
 oldRam8=cpu->map[0xff].write8;oldRam16=cpu->map[0xff].write16;cpu->map[0xff].write8=r8;cpu->map[0xff].write16=r16;
 oldSram8=cpu->map[0x20].write8;oldSram16=cpu->map[0x20].write16;cpu->map[0x20].write8=s8;cpu->map[0x20].write16=s16;
 f=fopen(argv[3],"w");if(!f)return 2;fputs("direct,ccr,sp,source_master,native_master,private_stack_bytes,native_write_bytes\n",f);
 unsigned pairs=0,directs=0,maxStack=0;const unsigned stacks[]={0,0xffff00,0xff1000};
 for(unsigned z=0;z<3;z++)for(unsigned ccr=0;ccr<32;ccr++){
  unsigned base=runCase(0,0,ccr,stacks[z],ccr+z*37);
  for(unsigned i=0;i<65536;i++)reference[i]=ram[i^1];memcpy(referenceSram,sram,65536);for(unsigned i=0;i<21;i++)referenceRegs[i]=getreg(i);
  unsigned native=runCase(1,0,ccr,stacks[z],ccr+z*37);
  for(unsigned i=0;i<16;i++)MUST(getreg(i)==referenceRegs[i]);MUST(getreg(17)==referenceRegs[17]&&getreg(19)==referenceRegs[19]);
  for(unsigned i=0;i<65536;i++)if(!((i>=BSS_BEGIN-0xff0000&&i<BSS_END-0xff0000)||(i>=0x7000&&i<0x7600)))MUST(ram[i^1]==reference[i]);
  for(unsigned i=0;i<65536;i++)if(i<0xf590||i>=0xf594)MUST(sram[i]==referenceSram[i]);
  if(0xff7600-minStack>maxStack)maxStack=0xff7600-minStack;
  fprintf(f,"0,%u,%u,%u,%u,%u,%u\n",ccr,stacks[z],base,native,0xff7600-minStack,writeCount);pairs++;
  /* Direct wrapper adds all32 possible incoming CCR combinations, beyond the
   * actual clear's two resulting X/Z combinations. Supervisor/mask7 only. */
  unsigned cost=runCase(1,1,ccr,stacks[z],ccr+z*37);
  for(unsigned i=0;i<15;i++)MUST(getreg(i)==(i==12?0x5b6:0x71b30000u+(ccr+z*37)*0x301+i*0x1101));
  MUST(getreg(17)==(0x2700|ccr));fprintf(f,"1,%u,%u,0,%u,%u,%u\n",ccr,stacks[z],cost,0xff7600-minStack,writeCount);directs++;
 }
 fclose(f);printf("PASS %u original6F2 clear/context pairs and %u direct wrapper CCR cases; exact D0-A7/SR/USP, source WRAM and SRAM outside explicit native owner ranges; all native writes bounded, max private stack %u bytes. No host state writes during execution. Boot-only supervisor mask7, no cadence or FPS claim.\n",pairs,directs,maxStack);return 0;
}
