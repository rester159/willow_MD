/* Closed timing fingerprints of the frozen baseline's exact guard instructions. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "guard-fixture.h"
static uint8_t seed[123392];static unsigned ticks[2],count;
static void(*old16)(unsigned,unsigned);
static void watch(unsigned a,unsigned v){if((a&0xffff)==0xf590&&count<2)ticks[count++]=cpu->cycles;if(old16)old16(a,v);else pw(sram+(a&65535),v);}
int main(int argc,char**argv){
 if(argc!=5)return 2;FILE*f=fopen(argv[2],"rb");if(!f)return 2;size_t n=fread(rom+0x102000,1,sizeof rom-0x102000,f);if(!n||!feof(f))return 2;fclose(f);
 f=fopen(argv[3],"rb");if(!f||fread(seed,1,sizeof seed,f)!=sizeof seed)return 2;fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x1f0000);const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x1f0000,boot,sizeof boot);memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 f=fopen(argv[4],"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();struct retro_game_info game={argv[4],rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;retro_run();m68k_set_irq(0);old16=cpu->map[0x20].write16;cpu->map[0x20].write16=watch;
 const char*names[]={"raw_busy","controller_active","palette_base","layer_control","near_base","far_base","priority_control","palette_not_ready","lease_plan_null","lease_epoch_null","lease_epoch_mismatch","source_epoch_mismatch","view_mismatch"};
 puts("predicate,phase3_to_failure_master,status");
 for(unsigned c=0;c<13;c++){
  for(unsigned i=0;i<65536;i++)ram[i^1]=seed[0xe200+i];memcpy(sram+0xf800,seed+0xd800,512);
  ww(0xff8004,2);ww(0xff8008,2);ww(0xff633a,4);ww(0xff6340,0);ww(0xff633e,1);ww(0xff4010,0);ww(0xff4014,0);
  pw(sram+0xf90a,0x91c0);pw(sram+0xf922,0x3e);pw(sram+0xf904,0x9180);pw(sram+0xf906,0x9140);pw(sram+0xf970,0x12f8);pw(sram+0xf56c,1);
  wl(0xff62f6,0x102000);wl(0xff62fa,0x20f568);wl(0xff62fe,7);pl(sram+0xf568,7);wl(0xff632a,9);wl(0xff4004,9);
  for(unsigned i=0;i<4;i++)pw(sram+0xf910+i*2,rw(0xff6332+i*2));
  switch(c){case 0:ww(0xff4010,1);break;case 1:ww(0xff4014,1);break;case 2:pw(sram+0xf90a,0);break;case 3:pw(sram+0xf922,0);break;case 4:pw(sram+0xf904,0);break;case 5:pw(sram+0xf906,0);break;case 6:pw(sram+0xf970,0);break;case 7:pw(sram+0xf56c,0);break;case 8:wl(0xff62f6,0);break;case 9:wl(0xff62fa,0);break;case 10:pl(sram+0xf568,8);break;case 11:wl(0xff4004,10);break;case 12:pw(sram+0xf910,rw(0xff6332)^1);break;}
  setreg(17,0x2700);setreg(15,0xff7700);setreg(16,PHASE3_PC);cpu->cycles=0;cpu->refresh=0x7fffffff;count=0;
  unsigned steps=0;while(count<2&&steps++<500)run68k(cpu->cycles+1);if(count!=2||word(sram+0xf590)!=(c==12?0x1505:0x1405))return 3;
  printf("%s,%u,%u\n",names[c],ticks[1]-ticks[0],word(sram+0xf592));
 }
 return 0;
}
