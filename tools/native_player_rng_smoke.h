/* Verify the ordinary player RNG callback against original 1D8E for every
 * possible state, including zero, while checking the observable scratch byte. */
static void nativeRngVideo(const void*data,unsigned w,unsigned h,size_t pitch)
{(void)data;(void)w;(void)h;(void)pitch;}
static int nativePlayerRandomCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg)return 2;
 FILE*f=fopen("build/player-rng-original.bin","rb");if(!f)return 2;
 WillowNativeBootGlobals layout;
 uint8_t*p=willowNativeBootGlobalsResolve(&layout,0x8284,3);if(!p){fclose(f);return 1;}
 unsigned at=symbol(symbols,"sourceBootGlobals")+(p-layout.bytes);
 unsigned char fixture[4],selected[4],expected[0xf600];
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");if(!setVideo){fclose(f);return 2;}
 setVideo(nativeRngVideo); /* Avoid rewriting the same diagnostic frame 65536 times. */
 for(unsigned seed=0;seed<65536;seed++){
  if(fread(fixture,1,4,f)!=4){fclose(f);return 2;}
  if(seed==0x1234)memcpy(selected,fixture,4);
  ram[at^1]=seed>>8;ram[(at+1)^1]=seed;ram[(at+2)^1]=(seed*73)^0xa5;
  memcpy(expected,ram,sizeof expected);
  for(unsigned n=0;n<3;n++)expected[(at+n)^1]=fixture[n];
  unsigned value=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"normalRandom",0,NULL);
  if(value==~0u || (value&255)!=fixture[3] || memcmp(expected,ram,sizeof expected)){
   fprintf(stderr,"Native RNG mismatch for seed %u\n",seed);fclose(f);return 1;
  }
 }
 if(fgetc(f)!=EOF||ferror(f)){fclose(f);return 2;}fclose(f);
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 uint8_t*sram=memory?memory(RETRO_MEMORY_SAVE_RAM):NULL;
 unsigned player=symbol(symbols,"debugPlayerRecord"),input=symbol(symbols,"sourceBootGlobals")+nativeBootGlobalOffset(0x806a);
 unsigned pool=symbol(symbols,"debugActorPools")+4*12,overflow=symbol(symbols,"debugIntroOverflow");
 unsigned indices=longword(ram,pool+4)&65535,records=longword(ram,pool);
 unsigned cursor=ram[(pool+10)^1],available=ram[(pool+11)^1];
 unsigned effect=records+ram[(indices+cursor)^1]*ram[(pool+9)^1];
 unsigned beforeOverflow=nativeSoundWord(ram,overflow),gate=0;
 if(available<=1)return 1;
 ram[input^1]=ram[(input+1)^1]=16;
 ram[(player+63)^1]=1;ram[(player+68)^1]=1;ram[(player+49)^1]=16;
 ram[at^1]=0x12;ram[(at+1)^1]=0x34;ram[(at+2)^1]=0xa5;
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"stepDispatchedPlayer",1,&gate);
 if(nativeSoundWord(ram,at)!=willowMotionWord(selected)||ram[(at+2)^1]!=selected[2]||
    ram[(pool+10)^1]!=cursor+1||ram[(pool+11)^1]!=available-1||*nativeTargetByte(ram,sram,effect)!=1||
    *nativeTargetByte(ram,sram,effect+32)!=0||*nativeTargetByte(ram,sram,effect+33)!=(selected[3]&6)||
    nativeSoundWord(ram,overflow)!=beforeOverflow){
  fprintf(stderr,"Native charge effect did not use source RNG after pool allocation\n");return 1;
 }
 /* The original <=1 availability guard must not consume random state. */
 ram[(pool+11)^1]=1;ram[(player+68)^1]=1;
 nativeSATCall(ram,symbols,setReg,getReg,runFrame,"stepDispatchedPlayer",1,&gate);
 if(nativeSoundWord(ram,at)!=willowMotionWord(selected)||ram[(at+2)^1]!=selected[2]||
    ram[(pool+10)^1]!=cursor+1||nativeSoundWord(ram,overflow)!=beforeOverflow)return 1;
 setVideo(video);
 puts("Native player charge effect: successful allocation consumes one shared draw and selects the source variant; reserve guard consumes none");
 puts("Native player RNG: all 65536 states match original CPU return byte, next state and scratch; unrelated non-stack RAM preserved");
 return 0;
}
