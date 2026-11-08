/* Closed timing fingerprints of the frozen baseline's exact guard instructions. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "profile-fixture.h"
static uint8_t seed[123392];static unsigned ticks[2],count;
static void(*old16)(unsigned,unsigned);
static void watch(unsigned a,unsigned v){if((a&0xffff)==0xf590&&count<2)ticks[count++]=cpu->cycles;if(old16)old16(a,v);else pw(sram+(a&65535),v);}
int main(int argc,char**argv){
 if(argc!=6)return 2;FILE*f=fopen(argv[2],"rb");if(!f)return 2;size_t n=fread(rom+0x102000,1,sizeof rom-0x102000,f);if(!n||!feof(f))return 2;fclose(f);
 f=fopen(argv[3],"rb");if(!f||fread(seed,1,sizeof seed,f)!=sizeof seed)return 2;fclose(f);
 f=fopen(argv[5],"rb");if(!f)return 2;size_t extra=fread(rom+0x110000,1,sizeof rom-0x110000,f);if(!extra||!feof(f))return 2;fclose(f);
 pl(rom,0xffff00);pl(rom+4,0x1f0000);const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x1f0000,boot,sizeof boot);memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 f=fopen(argv[4],"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();struct retro_game_info game={argv[4],rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;retro_run();m68k_set_irq(0);old16=cpu->map[0x20].write16;cpu->map[0x20].write16=watch;
 for(unsigned i=0;i<65536;i++)ram[i^1]=seed[0xe200+i];memcpy(sram+0xf800,seed+0xd800,512);
 setreg(17,0x2700);setreg(15,0xff75fc);wl(0xff75fc,0x1f0008);setreg(16,BLANK_PC);cpu->cycles=0;cpu->refresh=0x7fffffff;
 unsigned costs[sizeof(entries)/sizeof(*entries)]={0},calls[sizeof(entries)/sizeof(*entries)]={0};
 unsigned steps=0;
 while(getreg(16)!=0x1f0008&&steps++<100000){unsigned pc=getreg(16),ix=0;for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);i++)if(entries[i].pc<=pc)ix=i;unsigned before=cpu->cycles;if(pc==entries[ix].pc)calls[ix]++;run68k(cpu->cycles+1);costs[ix]+=cpu->cycles-before;}
 if(steps>=100000)return 3;
 printf("result,%u,total_master,%u\n",getreg(0),cpu->cycles);
 puts("function,calls,exclusive_master");for(unsigned i=0;i<sizeof(entries)/sizeof(*entries);i++)if(costs[i])printf("%s,%u,%u\n",entries[i].name,calls[i],costs[i]);

 return 0;
}
