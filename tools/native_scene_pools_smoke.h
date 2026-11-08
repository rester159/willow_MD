static int nativeScenePoolsCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned initialize)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 unsigned char*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],save[65536];
 if(!sram)return 2;
 memcpy(expected,ram,sizeof expected);memcpy(save,sram,sizeof save);
 if(initialize==2){
  setVideo(nativeRngVideo);unsigned records=symbol(symbols,"taskRecords");
  for(unsigned n=0;n<512;n+=32)if(expected[(records+n)^1]==1){
   unsigned at=(records+n+1)^1;expected[at]--;
   if(!expected[at])expected[(records+n)^1]=4;
  }
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(save,sram,sizeof save))return 1;
 }
 if(initialize){
  setVideo(nativeRngVideo);
  unsigned at=(symbol(symbols,"debugActorPools")+8)^1;
  unsigned old=ram[at];ram[at]=expected[at]=0;
  unsigned args[]={0x20,0};
  unsigned rejected=nativeSATCall(ram,symbols,setReg,getReg,runFrame,
   initialize==2?"sourceSceneBootTaskDispatch":"sourceSceneInitializeProbe",initialize==2?2:0,args);
  if(rejected!=(initialize==2?1:0)||memcmp(expected,ram,sizeof expected)||memcmp(save,sram,sizeof save))return 1;
  ram[at]=expected[at]=old;
 }
 const unsigned nativeOrder[]={4,0,1,5,6,2,7,3};
 const unsigned capacities[]={20,50,48,20,20,60,60,20,8};
 const unsigned strides[]={64,128,64,64,64,64,64,64,128};
 for(unsigned i=0;i<9;i++){
  unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:symbol(symbols,"debugActorPools")+nativeOrder[i]*12;
  unsigned records=longword(ram,pool),indices=longword(ram,pool+4)&65535;
  if(ram[(pool+8)^1]!=capacities[i]||ram[(pool+9)^1]!=strides[i])return 2;
  for(unsigned n=0;n<capacities[i];n++){
   for(unsigned b=0;b<strides[i];b++)*nativeTargetByte(expected,save,records+n*strides[i]+b)=b==30?(i+1)*2:0;
   expected[(indices+n)^1]=capacities[i]-1-n;
  }
  expected[(pool+10)^1]=0;expected[(pool+11)^1]=capacities[i];
 }
 const char*names[]={"debugPaletteControllers","debugStageObjects","debugPlayerRecord"};
 const unsigned sizes[]={1024,3072,128};
 for(unsigned i=0;i<3;i++)for(unsigned n=0;n<sizes[i];n++)expected[(symbol(symbols,names[i])+n)^1]=0;
 unsigned visibility=longword(ram,symbol(symbols,"debugClass12Visibility"))&65535;
 sceneCommitPut(expected,visibility+360,0x9646);sceneCommitPut(expected,visibility+362,0x96be);
 sceneCommitPut(expected,visibility+364,0xf97c);
 for(unsigned n=366;n<=368;n++)expected[(visibility+n)^1]=0;
 /* Independently sorted source bytes omitted from existing native owners. */
 const unsigned zero[]={0x83cb,0x97ff,0x9800,0x9802,0x9803,0x9805,0x9806,
  0x9808,0x9809,0x980b,0x980c,0x9811,0x9812,0x9814,0x9815,0xf48d,0xf48e,0xf89a,0xf99d};
 const unsigned pointers[][2]={{0x9818,0x937e},{0x981a,0x93a6},{0x981e,0x940a},{0x9820,0x946e},
  {0x9824,0x94ce},{0x9826,0x952e},{0x982a,0x9556},{0x982c,0x957e},
  {0x9830,0x95a6},{0x9832,0x95ce},{0x983c,0x9736},{0x983e,0x97ae},
  {0x9842,0x97d6},{0x9844,0x97fe},{0xf488,0xf476},{0xf48a,0xf486},{0xf898,0xf900}};
 unsigned used=0;
 for(unsigned at=0;at<65536;at++){
  int value=-1;
  for(unsigned n=0;n<sizeof zero/sizeof zero[0];n++)if(at==zero[n])value=0;
  for(unsigned n=0;n<sizeof pointers/sizeof pointers[0];n++){
   if(at==pointers[n][0])value=pointers[n][1]>>8;
   if(at==pointers[n][0]+1)value=pointers[n][1]&255;
  }
  if(value>=0)save[0x8000+used++]=value;
 }
 if(used!=53)return 2;
 if(initialize){
  unsigned globals=symbol(symbols,"sourceBootGlobals"),owner=symbol(symbols,"debugPaletteOwner");
  sceneCommitPut(expected,globals+24,(unsigned short)word(expected,globals+24)+2);
  sceneCommitPut(expected,globals+58,0x13b8);expected[(globals+145)^1]=0;
  for(unsigned n=0;n<4;n++)sceneCommitPut(expected,globals+60+n*2,bfpWord(0xa82+n*2));
  const unsigned sources[]={0xe9920,0xeb700,0xe72a0,0xe5280,0xe5080};
  const unsigned starts[]={64,96,32,16,0},counts[]={32,32,32,16,32};
  for(unsigned block=0;block<5;block++)for(unsigned n=0;n<counts[block];n++){
   unsigned g=starts[block]+n;
   sceneCommitPut(expected,owner+g*2,(sources[block]+n*32-0xe0000)/2);
   unsigned at=(owner+384+g/2)^1;expected[at]&=(g&1)?15:240;
  }
 }
 if(initialize==2){
  unsigned records=symbol(symbols,"taskRecords"),control=symbol(symbols,"sourceTaskState"),globals=symbol(symbols,"sourceBootGlobals");
  sceneCommitPut(expected,control+4,0xff);sceneCommitPut(expected,control+6,0x20);
  sceneCommitPut(expected,records+0x20,0x0101);sceneCommitPut(expected,records+0x22,0);
  expected[(globals+111)^1]|=12;
 }
 setVideo(nativeRngVideo);
 unsigned entry=fullSymbol(symbols,initialize==2?"sourceSceneBootTaskDispatch":initialize?"sourceSceneInitializeProbe":"sourceScenePoolsInitProbe"),pc=0xf800;
 if(initialize==2){
  sceneCommitPut(ram,pc,0x2f3c);sceneCommitPut(ram,pc+2,0);sceneCommitPut(ram,pc+4,0);pc+=6;
  sceneCommitPut(ram,pc,0x2f3c);sceneCommitPut(ram,pc+2,0);sceneCommitPut(ram,pc+4,0x20);pc+=6;
 }
 sceneCommitPut(ram,pc,0x4eb9);sceneCommitPut(ram,pc+2,entry>>16);sceneCommitPut(ram,pc+4,entry);pc+=6;
 if(initialize==2){sceneCommitPut(ram,pc,0x4fef);sceneCommitPut(ram,pc+2,8);pc+=4;}
 sceneCommitPut(ram,pc,0x60fe);unsigned stop=0xff0000+pc;
 setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
 unsigned frames=0;do{runFrame();frames++;}while(getReg(16)!=stop&&frames<8);
 unsigned result=getReg(16)==stop?(getReg(0)&65535):~0u;
 if(result!=(initialize==2?0x424f:1)||frames>(initialize?3:2)||memcmp(expected,ram,sizeof expected)||memcmp(save,sram,sizeof save)){
  fprintf(stderr,"Resident pool reset failed result %x frames %u\n",result,frames);
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Pool RAM %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}
  for(unsigned n=0;n<65536;n++)if(save[n]!=sram[n]){fprintf(stderr,"Pool SRAM %04x %02x/%02x\n",n,sram[n],save[n]);break;}
  return 1;
 }
 if(initialize==2){
  unsigned records=symbol(symbols,"taskRecords");
  for(unsigned n=0;n<512;n+=32)if(expected[(records+n)^1]==1){
   unsigned at=(records+n+1)^1;expected[at]--;
   if(!expected[at])expected[(records+n)^1]=4;
  }
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected))return 1;
  unsigned args[]={0x20,0},inner=symbol(symbols,"sourceBootGlobals")+24;
  unsigned old=(unsigned short)word(ram,inner);
  sceneCommitPut(ram,inner,4);sceneCommitPut(expected,inner,4);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(save,sram,sizeof save))return 1;
  sceneCommitPut(ram,inner,old);
 }
 setVideo(video);
 if(initialize==2)puts("Resident scene dispatch: original wake and slot-20 continuation execute 82A4, preserve live D2, clear D1 and sleep at 810E");
 if(initialize)puts("Resident 82A4 scene init: original 2304-word palette transfer sequence, masks, scene counter and pool reset verified together");
 printf("Resident 2648 pool reset: all nine pools, reverse free stacks, stage/player/palette records and 53 auxiliary bytes exact; visibility payloads and unrelated RAM/SRAM retained; %u observed frames (isolated reset bound, not a whole-game frame-budget pass)\n",frames);
 return 0;
}
