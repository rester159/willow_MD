/* Compare the terminal retained-memory clear independently of native fills. */
static int nativeOpeningClearCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],initial[0xf600],saved[65536],original[65536];
 if(!sram)return 2;
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned globals=symbol(symbols,"sourceBootGlobals"),map=symbol(symbols,"debugScroll2Map");
 unsigned nearAt=longword(ram,map)&65535,farAt=longword(ram,symbol(symbols,"debugScroll3Cells"))&65535;
 unsigned hud=symbol(symbols,"debugHudSurface"),flags=symbol(symbols,"sourceSceneClearedPlanes");
 unsigned prepared=symbol(symbols,"sourceScenePreparedPlanes");
 unsigned phase=(uint16_t)word(ram,globals+16),sub=(uint16_t)word(ram,globals+20);
 if(!((phase==0&&sub==10)||(phase==2&&(sub==12||sub==26))||(phase==4&&(sub==6||sub==32||sub==38)))||!word(ram,globals+143)||word(ram,map+14)||ram[(map+16)^1])return 2;
 unsigned nextPhase=phase==4?(sub==38?6:4):phase==2&&sub==12?2:phase+2,nextSub=phase==4?(sub==38?0:sub+2):phase==2&&sub==12?14:0;
 unsigned waitD1=bfpWord(phase==4?0x888e + sub:phase?0x8526+sub:0x8292);
 memcpy(expected,ram,sizeof expected);memcpy(initial,ram,sizeof initial);
 memcpy(saved,sram,sizeof saved);memcpy(original,sram,sizeof original);
 setVideo(nativeRngVideo);unsigned args[]={0x20,0};
 /* The pending-fade branch must preserve all video memory and live D2. */
 sceneCommitPut(ram,globals+143,0);sceneCommitPut(expected,globals+143,0);
 sceneCommitPut(expected,control+4,0xff);sceneCommitPut(expected,control+6,0x20);
 sceneCommitPut(expected,records+0x20,0x0101);sceneCommitPut(expected,records+0x22,waitD1);
 willowMotionPutWord(saved+0x8176,waitD1);
 willowMotionPutWord(saved+0x8178,word(ram,records+0x24));
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
  fputs("Resident dark-wait mismatch\n",stderr);return 1;
 }
 memcpy(ram,initial,sizeof initial);memcpy(expected,initial,sizeof expected);
 memcpy(sram,original,sizeof original);memcpy(saved,original,sizeof original);
 /* Source 65A4: Scroll1 pairs, near/far pairs, then two object banks. */
 for(unsigned n=0;n<4096;n++){
  willowMotionPutWord(saved+0x4000+n*4,0x7020);willowMotionPutWord(saved+0x4002+n*4,0);
  sceneCommitPut(expected,nearAt+n*2,WILLOW_SCROLL2_ZERO_PAIR);
 }
 for(unsigned n=0;n<288;n++){sceneCommitPut(expected,hud+n*4,0x7020);sceneCommitPut(expected,hud+n*4+2,0);}
 for(unsigned n=0;n<18;n++)sceneCommitPut(expected,hud+1152+n*2,n%3==2?255:65535);
 for(unsigned n=0;n<5632;n++)expected[(farAt+n)^1]=0;
 for(unsigned bit=0;bit<4096*11;bit++)expected[(farAt+bit/8)^1]|=((WILLOW_SCROLL3_CLEAR_PAIR>>(10-bit%11))&1)<<(7-bit%8);
 memset(saved,0,4096); /* Preserve displayed third bank and hardware mirrors. */
 sceneCommitPut(expected,globals+16,nextPhase);sceneCommitPut(expected,globals+20,nextSub);
 sceneCommitPut(expected,globals+112,0);sceneCommitPut(expected,globals+114,0);
 sceneCommitPut(expected,control+4,0xff);sceneCommitPut(expected,control+6,0x20);
 sceneCommitPut(expected,records+0x20,0x0101);sceneCommitPut(expected,records+0x22,0);sceneCommitPut(expected,records+0x24,0);
 expected[(globals+111)^1]|=12;expected[flags^1]|=11;expected[prepared^1]&=~11;
 willowMotionPutWord(saved+0x8176,0);willowMotionPutWord(saved+0x8178,0);
 unsigned clearResult=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args);
 if(clearResult!=0x424f||memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
  fprintf(stderr,"Resident opening clear mismatch result %x PC %x phase %u state %u dark %u\n",clearResult,getReg(16),(uint16_t)word(ram,globals+16),(uint16_t)word(ram,globals+20),(uint16_t)word(ram,globals+143));
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Clear RAM %x %x/%x\n",n^1,ram[n],expected[n]);break;}
  for(unsigned n=0;n<65536;n++)if(saved[n]!=sram[n]){fprintf(stderr,"Clear SRAM %x %x/%x\n",n,sram[n],saved[n]);break;}return 1;
 }
 for(unsigned n=0;n<512;n+=32)if(expected[(records+n)^1]==1){
  unsigned at=(records+n+1)^1;expected[at]--;if(!expected[at])expected[(records+n)^1]=4;
 }
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,40);sceneCommitPut(expected,globals+20,40);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
    memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 sceneCommitPut(ram,globals+20,nextSub);
 setVideo(video);
 printf("Resident clear %u/%u -> %u/%u: pending wait preserves video and source D1/D2; completed fade clears all retained maps/HUD/object banks, advances phase/substate and sleeps; unrelated RAM/SRAM exact, unsupported state rejects\n",phase,sub,nextPhase,nextSub);
 return 0;
}
