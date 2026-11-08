/* Original clock prefixes through the cartridge's retained source bus. */
static int nativeFrameClockCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/source-clock-original.bin","rb");if(!f)return 2;
 unsigned base=symbol(symbols,"sourceBootGlobals");
 unsigned irq=base+nativeBootGlobalOffset(0x8018),simulation=base+nativeBootGlobalOffset(0x8019);
 unsigned gate=base+nativeBootGlobalOffset(0x828f),input=base+nativeBootGlobalOffset(0x8068);
 unsigned char fixture[7],expected[0xf600];unsigned checks=0;
 setVideo(nativeRngVideo);
 while(fread(fixture,1,sizeof fixture,f)==sizeof fixture){
  ram[irq^1]=fixture[0];ram[simulation^1]=fixture[1];ram[gate^1]=fixture[2];ram[input^1]=fixture[3];
  memcpy(expected,ram,sizeof expected);
  /* Exercise both orders; each operation must leave the other byte alone. */
  for(unsigned phase=0;phase<2;phase++){
   unsigned interrupt=(phase^(checks&1))!=0;
   if(interrupt)expected[irq^1]=fixture[4];else expected[simulation^1]=fixture[5];
   unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
    interrupt?"sourceFrameInterruptProbe":"sourceFrameStepProbe",0,NULL);
   if(result==~0u || (!interrupt && (result&255)!=fixture[6]) || memcmp(expected,ram,sizeof expected)){
    fprintf(stderr,"Native frame clock fixture %u phase %u mismatch\n",checks,phase);fclose(f);return 1;
   }
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||checks!=8192)return 1;
 setVideo(video);
 puts("Native frame clock: 8192 original CPU fixtures, 16384 calls through retained source bus; both call orders, independent byte wrap, gates and unrelated RAM preserved");
 return 0;
}
