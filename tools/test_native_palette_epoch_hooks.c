/* Closed source-versus-hook calls, with before-call fixture seeds only.
 * Native SRAM bus remains unchanged; single-instruction stops only inspect.
 * This is not a continuous scheduler/cadence admission. */
#define main paletteUnusedContinuousMain
#include "test_native_cadence_compact_latch.c"
#undef main
#include "hooks-test.h"
#define FAIL() do {fprintf(stderr,"Palette hook assertion line%d\n",__LINE__);return 1;} while(0)

static const struct {unsigned at,target,body,end,words;const char*name;unsigned entry;} phooks[]={
 {0x95e,PALETTEEPOCHRAW,PALETTEEPOCHRAWBODY,PALETTEEPOCHRAWEND,512,"raw",0x95e},
 {0x9fc,PALETTEEPOCHCOPY,PALETTEEPOCHCOPYBODY,PALETTEEPOCHCOPYEND,512,"copy",0x9fc},
 {0xc92,PALETTEEPOCHANIMATED,PALETTEEPOCHANIMATEDBODY,PALETTEEPOCHANIMATEDEND,16,"animated",0xc92},
 {0x526e,PALETTEEPOCHFADE,PALETTEEPOCHFADEBODY,PALETTEEPOCHFADEEND,16,"fade",0x526e},
 {0x526e,PALETTEEPOCHFADE,PALETTEEPOCHFADEBODY,PALETTEEPOCHFADEEND,16,"fade_decision_write",0x5260},
 {0x526e,PALETTEEPOCHFADE,PALETTEEPOCHFADEBODY,PALETTEEPOCHFADEEND,0,"fade_decision_no_write",0x5260}
};
static uint8_t paletteBus[65536],basePalette[65536],baseRAM[65536],baseSRAM[65536];
static uint8_t *psram;static unsigned pv,pepoch,pwrites,pbusy;
static unsigned pr8(unsigned a){return paletteBus[a&65535];}
static unsigned pr16(unsigned a){return willowMotionWord(paletteBus+(a&65535));}
static void pcheck(void){
 if(pv&&(willowMotionLong(psram+0xf568)!=pepoch||willowMotionWord(psram+0xf56c)||willowMotionWord(psram+0xf56e)!=pbusy+1)){
  fprintf(stderr,"Source palette store before epoch invalidation or outside busy interval PC%x\n",cpu->pc);abort();
 }
 pwrites++;
}
static void pw8(unsigned a,unsigned v){pcheck();paletteBus[a&65535]=v;}
static void pw16(unsigned a,unsigned v){pcheck();willowMotionPutWord(paletteBus+(a&65535),v);}
static void ramput(unsigned a,unsigned v){for(unsigned n=0;n<4;n++)ram[((a+n)&65535)^1]=(uint8_t)(v>>(24-n*8));}

int main(int argc,char**argv){
 if(argc!=3)return 2;const char*out=argv[2];char path[1024];
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,0x100000,f)!=0x100000)return 2;fclose(f);
 snprintf(path,sizeof path,"%s/hooks.bin",out);f=fopen(path,"rb");if(!f)return 2;
 size_t bytes=fread(rom+PALETTEEPOCHRAW,1,sizeof rom-PALETTEEPOCHRAW,f);if(!bytes||!feof(f))return 2;fclose(f);
 uint8_t original[6][6];for(unsigned h=0;h<6;h++)memcpy(original[h],rom+phooks[h].at,6);
 willowMotionPutLong(rom,0xffff00);willowMotionPutLong(rom+4,0x100000);
 const uint8_t boot[]={0x13,0xfc,0,1,0,0xa1,0x30,0xf1,0x60,0xfe};memcpy(rom+0x100000,boot,sizeof boot);
 memcpy(rom+0x100,"SEGA GENESIS    ",16);memcpy(rom+0x1f0,"U               ",16);
 rom[0x1b0]='R';rom[0x1b1]='A';rom[0x1b2]=0xf8;rom[0x1b3]=0x20;
 willowMotionPutLong(rom+0x1b4,0x200000);willowMotionPutLong(rom+0x1b8,0x20ffff);
 snprintf(path,sizeof path,"%s/closed.bin",out);f=fopen(path,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,retro_run,(void));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={path,rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");psram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!psram)return 2;
 retro_run(); /* Native mapper enable; no host map override on SRAM. */
 if(cpu->pc!=m68k_get_reg(16))return 2;for(unsigned r=0;r<16;r++)if(cpu->dar[r]!=m68k_get_reg(r))return 2;
 cpu->map[0x91].base=paletteBus;cpu->map[0x91].read8=pr8;cpu->map[0x91].read16=pr16;cpu->map[0x91].write8=pw8;cpu->map[0x91].write16=pw16;
 snprintf(path,sizeof path,"%s/closed-cost.csv",out);f=fopen(path,"w");if(!f)return 2;
 fputs("family,original_bus,test,supervisor,ccr,source_master,hook_master,added_master,words,epoch\n",f);
 unsigned cases=0,observed=0;
 for(unsigned h=0;h<6;h++)for(unsigned bus=0;bus<2;bus++)for(unsigned test=0;test<128;test++){
  unsigned sr=((test&32)?0x2700:0)|(test&31),sp=(test&32)?0xffff00:0xff040c;
  unsigned count=h==1&&!(test&64)?256:phooks[h].words;
  unsigned destination=bus?0x91c000:0x20d800,base=bus?0xc000:0xd800;
  unsigned epoch=test==0?0xffffffffu:test==1?0:0x12345678u+test;
  unsigned regs[2][21],costs[2];
  for(unsigned variant=0;variant<2;variant++){
   pv=variant;pepoch=epoch+(count!=0);pwrites=0;pbusy=(test>>6)*3;
   for(unsigned n=0;n<65536;n++){ram[n^1]=(uint8_t)(n*73u+test*31u+h);psram[n]=(uint8_t)(n*109u+test*43u+h);paletteBus[n]=(uint8_t)(n*109u+test*43u+h);}
   /* Same 6KiB logical palette seed across original and native bus positions. */
   for(unsigned n=0;n<6144;n++){unsigned v=(uint8_t)(n*97u+test*17u+h);paletteBus[0xc000+n]=v;psram[0xd800+n]=v;}
   if(h>=4){unsigned first=((h==4)==!(test&64))?0xf123:0x0123;willowMotionPutWord(paletteBus+0xc000,first);willowMotionPutWord(psram+0xd800,first);}
   willowMotionPutLong(psram+0xf568,epoch);willowMotionPutWord(psram+0xf56c,1);willowMotionPutWord(psram+0xf56e,pbusy);
   uint8_t patch[6]={0x4e,0xf9};willowMotionPutLong(patch+2,phooks[h].target);
   for(unsigned n=0;n<6;n++)((uint8_t*)cpu->map[phooks[h].at>>16].base)[((phooks[h].at+n)&65535)^1]=(variant?patch:original[h])[n];
   m68k_set_reg(17,0x2700);m68k_set_reg(15,0xffff00);m68k_set_reg(19,0xff040c);m68k_set_reg(17,sr);
   for(unsigned r=0;r<15;r++)m68k_set_reg(r,0x12340000u+r*0x1111u+test);
   m68k_set_reg(1,0xbeef0000u|(count/16-1));m68k_set_reg(2,(test&64)?0xabcd1000:0xabcdf000);
   m68k_set_reg(8,h>=3?destination:0xe5080);m68k_set_reg(9,h>=3?0xff6000:destination);
   m68k_set_reg(15,sp);ramput(sp,0x100000);m68k_set_reg(16,phooks[h].entry);cpu->cycles=0;cpu->refresh=896;
   unsigned steps=0,bodySeen=0,endSeen=0;
   while(m68k_get_reg(16)!=0x100000&&steps++<10000){
    unsigned pc=m68k_get_reg(16);
    if(variant&&pc>=phooks[h].body&&pc<=phooks[h].end){
     if(willowMotionLong(psram+0xf568)!=pepoch||willowMotionWord(psram+0xf56c)||willowMotionWord(psram+0xf56e)!=pbusy+1){fprintf(stderr,"Metadata h%u bus%u test%u pc%x epoch%x/%x ready%x busy%x\n",h,bus,test,pc,willowMotionLong(psram+0xf568),pepoch,willowMotionWord(psram+0xf56c),willowMotionWord(psram+0xf56e));FAIL();}
     if(pc==phooks[h].body)bodySeen++;if(pc==phooks[h].end)endSeen++;
    }
    m68k_run(cpu->cycles+1);
   }
   if(steps>=10000)FAIL();
   for(unsigned r=0;r<21;r++)regs[variant][r]=m68k_get_reg(r);costs[variant]=cpu->cycles;
   if(bus&&pwrites!=count){fprintf(stderr,"Store count mismatch%u/%u\n",pwrites,count);FAIL();}
   if(!variant){for(unsigned n=0;n<65536;n++)baseRAM[n]=ram[n^1];memcpy(baseSRAM,psram,65536);memcpy(basePalette,paletteBus,65536);}
   else{
    if((count?(!bodySeen||endSeen!=1):(bodySeen||endSeen))||willowMotionLong(psram+0xf568)!=pepoch||willowMotionWord(psram+0xf56c)!=(count==0)||willowMotionWord(psram+0xf56e)!=pbusy)FAIL();
    observed+=bodySeen;
    for(unsigned r=0;r<21;r++)if(regs[0][r]!=regs[1][r]){fprintf(stderr,"Register mismatch family%u bus%u test%u r%u %x/%x\n",h,bus,test,r,regs[0][r],regs[1][r]);FAIL();}
    for(unsigned n=0;n<65536;n++){
     if((n<((sp-2)&65535)||n>=(sp&65535))&&ram[n^1]!=baseRAM[n])FAIL();
     if((n<0xf568||n>=0xf570)&&psram[n]!=baseSRAM[n])FAIL();
     if(paletteBus[n]!=basePalette[n])FAIL();
    }
    /* Independent original semantics for every destination word. */
    for(unsigned n=0;n<count;n++){
     unsigned seed=((uint8_t)(n*2*97u+test*17u+h)<<8)|(uint8_t)((n*2+1)*97u+test*17u+h);
     if(h>=4&&!n)seed=((h==4)==!(test&64))?0xf123:0x0123;
     unsigned expected=h>=3?(seed+((test&64)?0x1000:0xf000))&65535:willowMotionWord(rom+0xe5080+n*2);
     if(h==2)expected|=seed&0xf000;
     unsigned actual=willowMotionWord((bus?paletteBus:psram)+base+n*2);
     if(actual!=expected){fprintf(stderr,"Independent palette mismatch%u/%u\n",h,n);FAIL();}
    }
   }
  }
  fprintf(f,"%s,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",phooks[h].name,bus,test,(sr>>13)&1,sr&31,costs[0],costs[1],costs[1]-costs[0],count,epoch);cases++;
 }
 fclose(f);printf("Native palette epoch hooks: %u paired cases, all CCRs/user+supervisor, original reference and unchanged native SRAM buses; exact destination words, registers/SR/USP/SSP and all nonmetadata/non2-byte-stack memory. %u body observations prove ready0/incremented busy/new epoch before source stores. No-write fade paths preserve metadata; nested busy and epoch wrap checked. No host writes during calls.\n",cases,observed);
 return 0;
}
