#include "attract_replay_fixture.h"
/* Retained source replay phase, not a substitute demo/gameplay runner. */
static int nativeReplayPhaseCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/replay-phase-original.bin","rb");if(!f)return 2;
 unsigned globals=symbol(symbols,"sourceBootGlobals"),player=symbol(symbols,"debugPlayerRecord"),targets[22];
 for(unsigned n=0;n<22;n++)targets[n]=replayFixtureAddresses[n]==0x9846?player:
  globals+nativeBootGlobalOffset(replayFixtureAddresses[n]);
 unsigned char fixture[45],first[45],expected[0xf600];unsigned checks=0;
 setVideo(nativeRngVideo);
 while(fread(fixture,1,sizeof fixture,f)==sizeof fixture){
  if(!checks)memcpy(first,fixture,sizeof first);
  if(fixture[0]>1){fclose(f);return 1;}
  for(unsigned n=0;n<22;n++)ram[targets[n]^1]=fixture[1+n];
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<22;n++)expected[targets[n]^1]=fixture[23+n];
  unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
   fixture[0]?"sourceReplayRestoreProbe":"sourceReplayBeginProbe",0,NULL);
  if(result==~0u || (result&255)!=1 || memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Native replay phase fixture %u mismatch\n",checks);fclose(f);return 1;
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||checks!=4096)return 1;
 unsigned selector=globals+nativeBootGlobalOffset(0xe5a);
 const unsigned invalid[]={2,0xffff};
 for(unsigned n=0;n<2;n++){
  ram[selector^1]=invalid[n]>>8;ram[(selector+1)^1]=invalid[n];memcpy(expected,ram,sizeof expected);
  unsigned result=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceReplayBeginProbe",0,NULL);
  if(result==~0u || (result&255) || memcmp(expected,ram,sizeof expected))return 1;
 }
 /* Interleave real native clock/RNG operations between entry/restoration. */
 for(unsigned n=0;n<22;n++)ram[targets[n]^1]=first[1+n];
 unsigned clock=globals+nativeBootGlobalOffset(0x8018),rng=globals+nativeBootGlobalOffset(0x8284);
 unsigned oldClock=nativeSoundWord(ram,clock),oldRng=nativeSoundWord(ram,rng);
 ram[(globals+nativeBootGlobalOffset(0x828f))^1]=0;
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceReplayBeginProbe",0,NULL);
 for(unsigned n=0;n<8;n++){
  nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceFrameInterruptProbe",0,NULL);
  nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceFrameStepProbe",0,NULL);
  nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalRandom",0,NULL);
 }
 if(nativeSoundWord(ram,clock)==oldClock||nativeSoundWord(ram,rng)==oldRng)return 1;
 unsigned scratch=ram[(rng+2)^1];
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceReplayRestoreProbe",0,NULL);
 if(nativeSoundWord(ram,clock)!=oldClock||nativeSoundWord(ram,rng)!=oldRng||ram[(rng+2)^1]!=scratch)return 1;
 setVideo(video);
 puts("Native replay phase: 4096 original CPU entry/restore fixtures, both ROM descriptors, manager wrap, scratch and unrelated RAM preserved; invalid selectors reject; real clock/RNG advances restore to saved source phase");
 return 0;
}
