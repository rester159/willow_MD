/* Production cancellation binding against original trap-2 execution. */
static int nativePlayerCancelCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 if(!setReg||!setVideo)return 2;
 FILE*f=fopen("build/player-cancel-original.bin","rb");if(!f)return 2;
 unsigned records=symbol(symbols,"taskRecords"),state=symbol(symbols,"sourceTaskState");
 unsigned overflow=symbol(symbols,"debugIntroOverflow");
 unsigned char fixture[66],expected[0xf600];unsigned checks=0;
 setVideo(nativeRngVideo);
 for(unsigned n=0;n<32;n++)ram[(state+52+n)^1]=n*73+0x59;
 while(fread(fixture,1,sizeof fixture,f)==sizeof fixture){
  unsigned slot=willowMotionWord(fixture);if(slot>=512||(slot&31)){fclose(f);return 1;}
  for(unsigned n=0;n<512;n++)ram[(records+n)^1]=(n*7+checks*31)^0xa5;
  for(unsigned n=0;n<32;n++)ram[(records+slot+n)^1]=fixture[2+n];
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<32;n++)expected[(records+slot+n)^1]=fixture[34+n];
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalCancelTask",1,&slot)==~0u ||
     memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Native cancellation fixture %u slot %x mismatch\n",checks,slot);fclose(f);return 1;
  }
  checks++;
 }
 int bad=ferror(f);fclose(f);if(bad||checks!=4096)return 1;
 const unsigned invalid[]={1,31,33,511,512,0xffe0,0xffff};
 for(unsigned n=0;n<sizeof invalid/sizeof invalid[0];n++){
  memcpy(expected,ram,sizeof expected);unsigned count=nativeSoundWord(ram,overflow)+1;
  expected[overflow^1]=count>>8;expected[(overflow+1)^1]=count;
  nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalCancelTask",1,invalid+n);
  if(memcmp(expected,ram,sizeof expected)){fputs("Invalid task cancellation mutated unrelated state\n",stderr);return 1;}
 }
 /* Terminal player-removal fixture: original outer state six cancels C0. */
 unsigned player=symbol(symbols,"debugPlayerRecord"),script=symbol(symbols,"playerScript"),gate=0;
 unsigned char target[32];
 for(unsigned n=0;n<32;n++)target[n]=ram[(records+0xc0+n)^1]=(n*13+0x25);
 ram[(player+2)^1]=6;ram[(script+15)^1]=0;
 unsigned beforeOverflow=nativeSoundWord(ram,overflow);
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"stepDispatchedPlayer",1,&gate);
 target[0]=0;memset(target+16,0,16);
 for(unsigned n=0;n<32;n++)if(ram[(records+0xc0+n)^1]!=target[n])return 1;
 if(ram[player^1] || ram[(player+2)^1] || ram[(script+15)^1]!=1 ||
    nativeSoundWord(ram,overflow)!=beforeOverflow){fputs("Player removal did not finish its task cancellation\n",stderr);return 1;}
 setVideo(video);
 puts("Native player cancellation: 4096 original trap-2 fixtures across all slots/states; inactive records and native continuations preserved; seven invalid offsets reject; player removal cancels C0 without unavailable-service overflow");
 return 0;
}
