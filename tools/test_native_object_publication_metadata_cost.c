/* Closed native source-hook ABI/CCR, stack-canary and added-cycle proof.
 * Host seeds each independent case before entry; no host writes while running.
 * Continuous proof remains test_native_object_publication_metadata.c. */
#define main continuousMetadataMain
#include "test_native_object_publication_metadata.c"
#undef main
static const struct {unsigned at,size;uint8_t source[8];const char*name;unsigned stackBytes;} closedHooks[]={
 {0x3b550,8,{0x3e,0x3c,1,0,0x3b,0x47,2,0x9a},"begin",14},
 {0x3b56c,6,{8,0x6d,0,7,0,0x23},"double_publish",18},
 {0x3b564,6,{0x3b,0x7c,0x90,0,0,0x22},"single_publish",18},
 {0x590e,6,{0x30,0x3c,0,0xff,0x72,0},"clear",14},
 {0x10006e,6,{0x4e,0xb9,0,0x10,2,0x18},"acquire_and_latch",12}
};
static uint8_t closedSeedRAM[65536],closedBaselineRAM[65536],closedBaselineSRAM[65536];
int main(int argc,char**argv){
 if(argc!=2)return 2;const char*out="build/native-object-publication/metadata-v1";
 FILE*f=fopen("build/native-object-publication/metadata-v1/oracle.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 f=fopen("build/native-object-publication/metadata-v1/platform.bin","rb");if(!f)return 2;size_t bytes=fread(rom+0x100010,1,65536,f);if(!bytes||!feof(f))return 2;fclose(f);
 char path[512];snprintf(path,sizeof path,"%s/closed-native.bin",out);f=fopen(path,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(void,retro_run,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_set_irq,(unsigned));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={path,rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;
 retro_run();m68k_set_irq(0);
 unsigned cases=0;
 snprintf(path,sizeof path,"%s/closed-cost.csv",out);f=fopen(path,"w");if(!f)return 2;
 fputs("hook,case,bank,supervisor,ccr,remaining,source_master_cycles,native_master_cycles,added_master_cycles,ready\n",f);
 for(unsigned hook=0;hook<5;hook++){
  unsigned at=closedHooks[hook].at,size=closedHooks[hook].size;
  uint8_t target[8];memcpy(target,rom+at,size);
  unsigned count=hook==1||hook==2?2056:hook==4?128:128;
  for(unsigned test=0;test<count;test++){
   unsigned bank=hook==2?0:((test>>5)&1),super=hook==4?1:((test>>6)&1),ccr=test&31,remaining=hook==1||hook==2?test%257:test%257;
   unsigned sp=super?0xffff00:0xff040c,initialSR=(super?0x2700:0)|ccr;
   unsigned regs[2][21],cycles[2];
   for(unsigned variant=0;variant<2;variant++){
    uint8_t*code=cpu->map[at>>16].base;
    for(unsigned n=0;n<size;n++)code[((at+n)&65535)^1]=(variant?target:closedHooks[hook].source)[n];
    for(unsigned n=0;n<65536;n++){closedSeedRAM[n]=(uint8_t)(n*73u+test*31u+hook);ram[n^1]=closedSeedRAM[n];sram[n]=(uint8_t)(n*109u+test*43u+hook);}
    uint8_t*m=sram+METADATA_SRAM_OFFSET;memset(m,0,48);
    willowMotionPutLong(m,37);willowMotionPutWord(m+4,bank*8);willowMotionPutWord(m+6,1);
    willowMotionPutLong(m+8+bank*8,37);willowMotionPutWord(m+12+bank*8,256-remaining);willowMotionPutWord(m+14+bank*8,hook==4?(test>>6)&1:1);
    unsigned selector=bank?0x9080:0x9000;ram[0x8022^1]=0x90;ram[0x8023^1]=hook==1?(selector^0x80)&255:0;
    willowMotionPutWord(sram+0xf900,selector);willowMotionPutLong(sram+0xf028,0);
    m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(19,0xff040c);m68k_set_reg(17,initialSR);
    for(unsigned r=0;r<15;r++)m68k_set_reg(r,0x12340000u+r*0x1111u+test);
    m68k_set_reg(6,0x45670000u|remaining);m68k_set_reg(8,bank?0x20d000:0x20c000);m68k_set_reg(13,0xff8000);m68k_set_reg(15,sp);m68k_set_reg(16,at);
    cpu->cycles=0;cpu->refresh=896;
    unsigned steps=0;while(m68k_get_reg(16)!=at+size&&steps++<10000)m68k_run(cpu->cycles+1);
    if(steps>=10000){fprintf(stderr,"Closed hook%u stuck PC%x SR%x\n",hook,m68k_get_reg(16),m68k_get_reg(17));return 1;}
    for(unsigned r=0;r<21;r++)regs[variant][r]=m68k_get_reg(r);cycles[variant]=(unsigned)cpu->cycles;
    if(!variant){for(unsigned n=0;n<65536;n++)closedBaselineRAM[n]=ram[n^1];memcpy(closedBaselineSRAM,sram,65536);}
    else{
     for(unsigned r=0;r<21;r++){
      if(hook==4&&(r==0||r==1||r==8||r==9||r==17))continue;
      if(regs[1][r]!=regs[0][r]){fprintf(stderr,"Closed hook%u test%u reg%u source%x native%x\n",hook,test,r,regs[0][r],regs[1][r]);return 1;}
     }
     if(hook==4&&(regs[0][17]&0xff00)!=(regs[1][17]&0xff00))return 1;
     unsigned stackStart=(sp-closedHooks[hook].stackBytes)&65535,stackEnd=sp&65535;
     for(unsigned n=0;n<65536;n++){
      if((n<stackStart||n>=stackEnd)&&ram[n^1]!=closedBaselineRAM[n]){fprintf(stderr,"Closed hook%u test%u outside exact call stack RAM%x differs\n",hook,test,n);return 1;}
      if((n<METADATA_SRAM_OFFSET||n>=METADATA_SRAM_OFFSET+48)&&sram[n]!=closedBaselineSRAM[n]){fprintf(stderr,"Closed hook%u test%u nonmetadata SRAM%x differs\n",hook,test,n);return 1;}
     }
     if(willowMotionLong(m+44)){fprintf(stderr,"Closed metadata fault hook%u test%u\n",hook,test);return 1;}
     if(hook==0&&(willowMotionLong(m)!=38||willowMotionLong(m+8+bank*8)!=38||willowMotionWord(m+14+bank*8)||willowMotionWord(m+6)!=1))return 1;
     if((hook==1||hook==2)&&(willowMotionWord(m+12+bank*8)!=256-remaining||willowMotionWord(m+14+bank*8)!=1||willowMotionWord(m+6)))return 1;
     if(hook==3&&willowMotionWord(m+14+bank*8))return 1;
     if(hook==4){unsigned ready=(test>>6)&1;if(willowMotionWord(m+32)!=ready||willowMotionWord(m+30)!=bank)return 1;if(ready&&(willowMotionLong(m+24)!=37||willowMotionWord(m+28)!=256-remaining))return 1;}
    }
   }
   fprintf(f,"%s,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",closedHooks[hook].name,test,bank,super,ccr,remaining,cycles[0],cycles[1],cycles[1]-cycles[0],hook==4?(test>>6)&1:1);cases++;
  }
 }
 fclose(f);
 printf("Closed native metadata: %u paired source/hook cases; source register/CCR/USP/SSP effects and all non-call-stack RAM/nonmetadata SRAM exact. Both original user/supervisor modes, both banks, every CCR and remaining count0..256; full raw latch retained in acquire comparison. No host writes during calls.\n",cases);
 return 0;
}
