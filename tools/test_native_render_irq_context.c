/* Real 68000 TRAP/RTE differential: logical source IRQ inputs versus an
 * interrupted native context. Host setup occurs only before closed calls. */
#define WILLOW_CLOSED_HELPERS_ONLY 1
#include "test_native_render_cache_closed.c"
#include "irq-context-fixture.h"
static uint8_t sourceRam[65536],sourceSram[65536],referenceRam[65536],referenceSram[65536];
static unsigned actual[21],logical[15];
static int load(const char*directory,const char*name,uint8_t*dst,size_t size){
 char path[4096];snprintf(path,sizeof path,"%s/%s",directory,name);FILE*f=fopen(path,"rb");if(!f)return 0;
 size_t n=fread(dst,1,size,f);fclose(f);return n==size;
}
static void patchVector(unsigned destination){
 uint8_t bytes[4];pl(bytes,destination);
 for(unsigned n=0;n<4;n++)((uint8_t*)cpu->map[0].base)[(0xb4+n)^1]=bytes[n];
}
static void irqCall(unsigned bridge,unsigned contextReady,unsigned supervisor,unsigned ccr,unsigned fixture){
 unsigned nativeCCR=ccr^31,sourceSP=0xff6000,flags=contextReady?nativeCCR:ccr;
 for(unsigned n=0;n<65536;n++)ram[n^1]=sourceRam[n];memcpy(sram,sourceSram,65536);
 ww(CONTEXT_READY,contextReady==1);wl(PREPARE_SP,sourceSP);
 for(unsigned n=0;n<15;n++){
  logical[n]=0x28460000u+n*0x10101u+fixture*0x101;
  wl(sourceSP+n*4,logical[n]);
 }
 logical[13]=0xffff8000;wl(sourceSP+13*4,logical[13]);
 /* Both bits distinguish byte-only source4D86 from accidental clear/load. */
 logical[4]=(logical[4]&0xffff00ffu)|((fixture&1)?0x5a00:0xa500);wl(sourceSP+4*4,logical[4]);
 ww(sourceSP+60,ccr);
 /* Real source controls, deliberately chosen legal diagnostic branches. */
 ww(0xff8022,0x9080);ram[0x82a1^1]=1;ram[0x8349^1]=(fixture&1)?1:0;
 ram[0x83ce^1]=0;ram[0x835c^1]=1;ram[0x83d6^1]=(fixture&2)?255:0;
 ww(0xff801e,2);ww(0xff8020,0);ww(0xff8172,0x1234);
 if(fixture&4){ram[0x82a1^1]=0;ram[0x8283^1]=1;}
 patchVector(bridge?0x1f0020:0x1f0010);
 setreg(17,0x2700);setreg(15,0xffff00);setreg(19,0xff7400);
 unsigned chosenCCR=bridge?flags:ccr;
 setreg(17,(supervisor?0x2000:0)|chosenCCR);
 for(unsigned n=0;n<15;n++){
  unsigned poisoned=contextReady==1||(contextReady==2&&(n==0||n==1||n==8||n==9));
  unsigned v=bridge&&poisoned?(0x109000u+n*0x121u+fixture):logical[n];
  actual[n]=v;setreg(n,v);
 }
 actual[15]=supervisor?0xffff00:0xff7400;
 actual[17]=(supervisor?0x2000:0)|chosenCCR;
 setreg(16,0x1f0000);cpu->cycles=0;cpu->refresh=896;
 unsigned steps=0;
 while(getreg(16)!=0x1f0002&&steps++<100000)run68k(cpu->cycles+1);
 if(steps>=100000){fprintf(stderr,"IRQ failed to return bridge%u ready%u fixture%u pc%x\n",bridge,contextReady,fixture,getreg(16));exit(3);}
 for(unsigned n=0;n<15;n++)if(getreg(n)!=actual[n]){fprintf(stderr,"IRQ register%u %x/%x\n",n,getreg(n),actual[n]);exit(3);}
 if(getreg(15)!=actual[15]||getreg(17)!=actual[17]||getreg(19)!=0xff7400){fprintf(stderr,"IRQ SP/SR/USP changed %x %x %x\n",getreg(15),getreg(17),getreg(19));exit(3);}
 /* Source epilogue must leave actual native context in its exact saved image. */
 for(unsigned n=0;n<15;n++)if(rl(0xfffebe + n*4)!=actual[n]){fprintf(stderr,"Saved register image differs r%u\n",n);exit(3);}
 if(rw(0xfffefa)!=actual[17]||rl(0xfffefc)!=0x1f0002){fprintf(stderr,"Exception image differs\n");exit(3);}
}
int main(int argc,char**argv){
 if(argc!=4)return 2;
 if(!load(argv[2],"program.bin",rom,0x100000))return 2;
 FILE*f;char path[4096];snprintf(path,sizeof path,"%s/platform.bin",argv[2]);f=fopen(path,"rb");if(!f)return 2;
 size_t n=fread(rom+0x100010,1,65520,f);if(!n||!feof(f))return 2;fclose(f);
 if(!load(argv[2],"source-ram.bin",sourceRam,65536)||!load(argv[2],"source-sram.bin",sourceSram,65536))return 2;
 pl(rom,0xffff00);pl(rom+4,0x100000);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x100000,boot,sizeof boot);
 rom[0x1f0000]=0x4e;rom[0x1f0001]=0x4d;rom[0x1f0002]=0x60;rom[0x1f0003]=0xfe;
 const uint8_t carrier[]={0x02,0x7c,0xf8,0xff,0x00,0x7c,0x22,0x00,0x4e,0xf9};
 memcpy(rom+0x1f0010,carrier,10);pl(rom+0x1f001a,0x1554);
 memcpy(rom+0x1f0020,carrier,10);pl(rom+0x1f002a,IRQ_BRIDGE);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;pl(rom+0x1b4,0x200000);pl(rom+0x1b8,0x20ffff);
 snprintf(path,sizeof path,"%s/closed-irq.bin",argv[2]);f=fopen(path,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));FN(void,m68k_set_irq,(unsigned));
 setreg=dlsym(core,"m68k_set_reg");getreg=dlsym(core,"m68k_get_reg");run68k=dlsym(core,"m68k_run");if(!setreg||!getreg||!run68k)return 2;
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={path,rom,sizeof rom,0};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;
 retro_run();m68k_set_irq(0);
 f=fopen(argv[3],"w");if(!f)return 2;fputs("fixture,ready,supervisor,ccr,source_master,bridge_master,added_master\n",f);
 unsigned pairs=0,dirty=0;
 for(unsigned fixture=0;fixture<8;fixture++)for(unsigned ready=0;ready<3;ready++)for(unsigned sup=0;sup<2;sup++)for(unsigned ccr=0;ccr<32;ccr++){
  unsigned originalCost=0;
  for(unsigned bridge=0;bridge<2;bridge++){
   irqCall(bridge,ready,sup,ccr,fixture);
   if(!bridge){for(unsigned n=0;n<65536;n++)referenceRam[n]=ram[n^1];memcpy(referenceSram,sram,65536);originalCost=(unsigned)cpu->cycles;}
   else{
    for(unsigned n=0;n<65536;n++)if(!(n>=0xfebe&&n<0xff00)&&ram[n^1]!=referenceRam[n]){fprintf(stderr,"Source RAM differs fixture%u ready%u S%u CCR%u at%x %x/%x\n",fixture,ready,sup,ccr,n,ram[n^1],referenceRam[n]);return 1;}
    if(memcmp(sram,referenceSram,65536)){for(unsigned n=0;n<65536;n++)if(sram[n]!=referenceSram[n]){fprintf(stderr,"Source SRAM differs fixture%u ready%u at%x %x/%x\n",fixture,ready,n,sram[n],referenceSram[n]);break;}return 1;}
    if(word(sram+0xf900)!=0x9080){fprintf(stderr,"Wrong source bank selector\n");return 1;}
    if(fixture&1){if((word(sram+0x0df2)&0xff00)!=0x5a00){fprintf(stderr,"D4 high-byte semantics missing %x\n",word(sram+0x0df2));return 1;}dirty++;}
    fprintf(f,"%u,%u,%u,%u,%u,%u,%d\n",fixture,ready,sup,ccr,originalCost,(unsigned)cpu->cycles,cpu->cycles-(int)originalCost);pairs++;
   }
  }
 }
 fclose(f);printf("PASS %u real TRAP/RTE pairs; all CCR/user/supervisor/source-entry/native-ready/return-window combinations, full source RAM outside independently checked66-byte exception+register record and all64KiB native SRAM exact; restored actual native registers/SR/SP/USP; %u dirty-HUD checks preserve logical D4high byte. Return-window mode poisons caller-clobbered D0/D1/A0/A1 andCCR with context-ready clear, retaining C-ABI-preserved source regs. No host writes inside IRQ execution.\n",pairs,dirty);return 0;
}
