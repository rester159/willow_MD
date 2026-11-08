/* Closed diagnostic of the exact cadence-v2 added call/validation sequence.
 * Host seeds this isolated fixture before entry; no host changes occur while
 * it executes. The continuous cadence test remains the separate no-register-
 * writes test. This probe uses the unchanged Genesis core bus throughout. */
#define main cadenceHarnessMain
#include "test_native_cadence_latch.c"
#undef main
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen("build/native-cadence/latch-v2/oracle.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 f=fopen("build/native-cadence/latch-v2/platform.bin","rb");if(!f)return 2;size_t bytes=fread(rom+0x100010,1,65536,f);if(!bytes||!feof(f))return 2;fclose(f);
 unsigned call=0;for(unsigned at=0x100010;at+18<0x100010+bytes;at+=2)if(willowMotionWord(rom+at)==0x4eb9&&willowMotionLong(rom+at+2)==CADENCE_LATCH){if(call)return 2;call=at;}
 if(!call||willowMotionLong(rom+call+6)!=0x4a006600||willowMotionWord(rom+call+10)!=8)return 2;
 unsigned done=call+18;
 f=fopen("build/native-cadence/latch-v2/cost-native.bin","wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(void,retro_run,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={"build/native-cadence/latch-v2/cost-native.bin",rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;
 retro_run(); /* Run the real platform startup through the core frame API before closed-fixture seeding. */
 unsigned times[8];
 for(unsigned test=0;test<8;test++){
  unsigned bank=test&1,selector=bank?0x9080:0x9000,bankOffset=bank?0xd000:0xc000;
  for(unsigned at=0;at<65536;at++){sourceRAM[at]=(uint8_t)(at*73u+test*31u);ram[at^1]=sourceRAM[at];sram[at]=(uint8_t)(at*109u+test*43u);}
  willowMotionPutWord(sram+0xf900,selector);willowMotionPutLong(sram+0xf028,0);
  for(unsigned r=0;r<15;r++)m68k_set_reg(r,0x12340000u+r*0x1111u+test);
  m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(16,call);cpu->cycles=0;cpu->refresh=896;
  unsigned steps=0;while(m68k_get_reg(16)!=done&&steps++<10000)m68k_run(cpu->cycles+1);
  if(steps>=10000||m68k_get_reg(15)!=0xffff00||willowMotionLong(sram+0xf028)!=1||willowMotionWord(sram+0xf02c)!=selector){fprintf(stderr,"Cost stop steps%u PC%x SP%x count%u selector%x wanted%x\n",steps,m68k_get_reg(16),m68k_get_reg(15),willowMotionLong(sram+0xf028),willowMotionWord(sram+0xf02c),selector);return 1;}
  for(unsigned r=2;r<15;r++)if(r!=8&&r!=9&&m68k_get_reg(r)!=(0x12340000u+r*0x1111u+test)){fprintf(stderr,"Callee-preserved register%u changed\n",r);return 1;}
  for(unsigned at=0;at<65536;at++){
   int offset=latchByte(at);unsigned expected=offset>=0?sram[bankOffset+(unsigned)offset]:sourceRAM[at];
   if(offset<0&&at>=0xfef8&&at<0xff00)continue; /* Exact eight bytes used by JSR return and callee A2 save. */
   if(ram[at^1]!=expected){fprintf(stderr,"RAM/canary mismatch%x\n",at);return 1;}
  }
  times[test]=(unsigned)cpu->cycles;
  printf("Exact cadence call bank%u: %u mastercycles, %.0f CPUcycles, %.9fms; preserved D2-D7/A2-A6/SP and all non-latch/non-call-stack RAM\n",bank,times[test],times[test]/7.0,times[test]*1000.0/53693175);
 }
 f=fopen("build/native-cadence/latch-v2/capture-cost.json","w");if(!f)return 2;
 fprintf(f,"{\"scope\":\"Exact added native JSR/helper/RTS/TST/BNE sequence; unchanged Genesis bus, closed fixture\",\"call_pc\":%u,\"done_pc\":%u,\"master_hz\":53693175,\"master_cycles\":[",call,done);
 for(unsigned n=0;n<8;n++)fprintf(f,"%s%u",n?",":"",times[n]);fputs("],\"banks\":[0,1,0,1,0,1,0,1],\"ram_bytes_checked_per_case\":65528,\"capture_bytes\":2048,\"host_writes_during_call\":false,\"custom_bus_callbacks\":false,\"physical_hardware_proven\":false}\n",f);fclose(f);return 0;
}
