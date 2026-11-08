#include "source_input_fixture.h"
static int nativeSourceInputCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/source-input-original.bin","rb");if(!f)return 2;
 unsigned globals=symbol(symbols,"sourceBootGlobals"),targets[13];
 for(unsigned n=0;n<13;n++)targets[n]=globals+nativeBootGlobalOffset(inputFixtureAddresses[n]);
 unsigned char fixture[26],expected[0xf600];unsigned checks=0;
 setVideo(nativeRngVideo);
 while(fread(fixture,1,sizeof fixture,f)==sizeof fixture){
  /* Replay frames after the first inherit actual prior cartridge output. */
  for(unsigned n=0;n<13;n++){
   if(checks%1024>128 && (n<2 || n>=7)){
    if(ram[targets[n]^1]!=fixture[n]){fclose(f);return 1;}
   }else ram[targets[n]^1]=fixture[n];
  }
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<13;n++)expected[targets[n]^1]=fixture[13+n];
  unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalSourceInputSample",0,NULL);
  if(result==~0u || (result&255)!=1 || memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Native source input fixture %u mismatch\n",checks);fclose(f);return 1;
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||checks!=2048)return 1;
 /* Countdown-only frames must not dereference an unavailable replay pointer. */
 ram[targets[2]^1]=1;
 const unsigned char unavailable[4]={0xde,0xad,0xbe,0xee};
 for(unsigned n=0;n<4;n++)ram[targets[9+n]^1]=unavailable[n];
 const unsigned timers[]={0,2,65535};
 for(unsigned n=0;n<3;n++){
  ram[targets[7]^1]=timers[n]>>8;ram[targets[8]^1]=timers[n];
  memcpy(expected,ram,sizeof expected);expected[targets[1]^1]=ram[targets[0]^1];
  unsigned after=(timers[n]-1)&65535;expected[targets[7]^1]=after>>8;expected[targets[8]^1]=after;
  unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalSourceInputSample",0,NULL);
  if(result==~0u || (result&255)!=1 || memcmp(expected,ram,sizeof expected))return 1;
 }
 ram[targets[7]^1]=0;ram[targets[8]^1]=1;memcpy(expected,ram,sizeof expected);
 unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalSourceInputSample",0,NULL);
 if(result==~0u || (result&255) || memcmp(expected,ram,sizeof expected))return 1;
 setVideo(video);
 puts("Native source input: 2048 original CPU fixtures; live port/cabinet routing and 896 consecutive frames per ROM replay, previous-input ordering, timer wrap and unrelated RAM preserved; unavailable record rejects before mutation");
 return 0;
}
