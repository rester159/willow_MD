/* Actual native IRQ-callee path, bounded prefix versus full metadata-v1 latch.
 * All fixture seeding is before calls. Target bus remains unchanged. */
#define main continuousCompactMain
#include "test_native_cadence_compact_latch.c"
#undef main
static uint8_t seed[65536];
int main(int argc,char**argv){
 if(argc!=2)return 2;const char*out="build/native-object-publication/compact-v1";
 FILE*f=fopen("build/native-object-publication/compact-v1/oracle.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 f=fopen("build/native-object-publication/compact-v1/platform.bin","rb");if(!f)return 2;size_t bytes=fread(rom+0x100010,1,65536,f);if(!bytes||!feof(f))return 2;fclose(f);
 unsigned innerCall=0;for(unsigned at=METADATAACQUIREANDLATCH;at<METADATAACQUIREANDLATCH+12;at+=2)if(willowMotionWord(rom+at)==0x4eb9&&willowMotionLong(rom+at+2)==CADENCE_LATCH){if(innerCall)return 2;innerCall=at;}
 if(!innerCall)return 2;
 char path[512];snprintf(path,sizeof path,"%s/closed-native.bin",out);f=fopen(path,"wb");if(!f||fwrite(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 void*core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(retro_environment_t));FN(void,retro_set_video_refresh,(retro_video_refresh_t));FN(void,retro_set_audio_sample,(retro_audio_sample_t));FN(void,retro_set_audio_sample_batch,(retro_audio_sample_batch_t));FN(void,retro_set_input_poll,(retro_input_poll_t));FN(void,retro_set_input_state,(retro_input_state_t));FN(void,retro_init,(void));FN(void,retro_run,(void));FN(bool,retro_load_game,(const struct retro_game_info*));FN(void,m68k_set_reg,(int,unsigned));FN(unsigned,m68k_get_reg,(int));FN(void,m68k_set_irq,(unsigned));FN(void,m68k_run,(unsigned));FN(void*,retro_get_memory_data,(unsigned));
 retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 struct retro_game_info game={path,rom,sizeof rom,NULL};if(!retro_load_game(&game))return 2;
 ram=dlsym(core,"work_ram");cpu=dlsym(core,"m68k");uint8_t*sram=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);if(!ram||!cpu||!sram)return 2;
 retro_run();m68k_set_irq(0);
 snprintf(path,sizeof path,"%s/capture-cost.csv",out);f=fopen(path,"w");if(!f)return 2;fputs("records,bank,proved,full_metadata_master,compact_master,saved_master\n",f);
 unsigned cases=0;
 for(unsigned proved=0;proved<2;proved++)for(unsigned bank=0;bank<2;bank++)for(unsigned records=0;records<=256;records++){
  unsigned cycles[2];
  for(unsigned variant=0;variant<2;variant++){
   unsigned destination=variant?CADENCE_LATCH:0x100218;uint8_t*code=cpu->map[innerCall>>16].base;
   for(unsigned n=0;n<4;n++)code[((innerCall+2+n)&65535)^1]=(uint8_t)(destination>>(24-n*8));
   for(unsigned n=0;n<65536;n++){seed[n]=(uint8_t)(n*73u+records*31u+bank);ram[n^1]=seed[n];sram[n]=(uint8_t)(n*109u+records*43u+bank);}
   uint8_t*m=sram+METADATA_SRAM_OFFSET;memset(m,0,56);
   willowMotionPutLong(m+8+bank*8,1234);willowMotionPutWord(m+12+bank*8,records);willowMotionPutWord(m+14+bank*8,proved);
   willowMotionPutWord(sram+0xf900,bank?0x9080:0x9000);willowMotionPutLong(sram+0xf028,0);
   m68k_set_reg(17,0x2700|(records&31));m68k_set_reg(15,0xffff00);
   for(unsigned r=0;r<15;r++)m68k_set_reg(r,0x12340000u+r*0x1111u+records);
   m68k_set_reg(16,0x10006e);cpu->cycles=0;cpu->refresh=896;
   unsigned steps=0;while(m68k_get_reg(16)!=0x100074&&steps++<10000)m68k_run(cpu->cycles+1);
   if(steps>=10000||m68k_get_reg(15)!=0xffff00||(m68k_get_reg(0)&255)!=1||willowMotionLong(sram+0xf028)!=1||willowMotionLong(m+44)){fprintf(stderr,"Compact closed call fault\n");return 1;}
   for(unsigned r=2;r<15;r++)if(r!=8&&r!=9&&m68k_get_reg(r)!=(0x12340000u+r*0x1111u+records)){fprintf(stderr,"Compact C-ABI register%u changed\n",r);return 1;}
   if((m68k_get_reg(17)&0xff00)!=0x2700)return 1;
   unsigned copied=variant&&proved?records:256,bankOffset=bank?0xd000:0xc000;
   for(unsigned n=0;n<65536;n++){
    int off=latchByte(n);
    unsigned expected=off>=0&&(unsigned)off<copied*8?sram[bankOffset+(unsigned)off]:seed[n];
    /* Two return addresses,12 bytes compiler locals andD2/A2 saves:28bytes. */
    if(n>=0xfee4&&n<0xff00)continue;
    if(ram[n^1]!=expected){fprintf(stderr,"Closed prefix count%u bank%u proved%u variant%u RAM%x canary differs\n",records,bank,proved,variant,n);return 1;}
   }
   for(unsigned n=0;n<65536;n++){
    if(n>=0xf500&&n<0xf538)continue; /* Independently checked descriptor/metadata. */
    unsigned expected=(uint8_t)(n*109u+records*43u+bank);
    if(n>=0xf028&&n<0xf02c)expected=n==0xf02b?1:0;
    if(n==0xf02c||n==0xf900)expected=0x90;
    if(n==0xf02d||n==0xf901)expected=bank?0x80:0;
    if(sram[n]!=expected){fprintf(stderr,"Closed prefix nonmetadata/source-bank SRAM%x canary differs\n",n);return 1;}
   }
   if(variant&&(willowMotionWord(sram+0xf530)!=copied||willowMotionWord(sram+0xf532)!=proved||willowMotionLong(sram+0xf534)!=(proved?1234:0)))return 1;
   cycles[variant]=(unsigned)cpu->cycles;cases++;
  }
  fprintf(f,"%u,%u,%u,%u,%u,%d\n",records,bank,proved,cycles[0],cycles[1],(int)cycles[0]-(int)cycles[1]);
 }
 fclose(f);printf("Native compact prefix: %u closed calls, every count0..256, both banks, proved and full-fallback paths. All65508 non-call-stack RAM bytes, copied prefix, untouched tail, C ABI and actual published capture descriptor exact; no host writes inside calls.\n",cases);return 0;
}
