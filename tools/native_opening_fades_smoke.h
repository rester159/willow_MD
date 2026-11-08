/* Dispatch the actually allocated dynamic fades in source slot order. */
static int nativeOpeningFadesCheck(void*core,uint8_t*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 if(!setReg||!setVideo||!memory)return 2;
 uint8_t*sram=memory(RETRO_MEMORY_SAVE_RAM),expected[0xf600],saved[65536];
 if(!sram)return 2;
 unsigned control=symbol(symbols,"sourceTaskState"),records=symbol(symbols,"taskRecords");
 unsigned globals=symbol(symbols,"sourceBootGlobals"),owner=symbol(symbols,"debugPaletteOwner");
 unsigned slots=symbol(symbols,"debugPaletteSlots");uint16_t bases[3072];
 memcpy(expected,ram,sizeof expected);memcpy(saved,sram,sizeof saved);
 memset(nptSource,0,sizeof nptSource);
 const unsigned src[]={0,0x200,0xa10},dst[]={records,control,control+16},sizes[]={512,16,32};
 for(unsigned r=0;r<3;r++)for(unsigned n=0;n<sizes[r];n++)nptSource[src[r]+n]=ram[(dst[r]+n)^1];
 for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
  const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
  for(unsigned n=0;n<g->count;n++)nptSource[g->source+n]=ram[(globals+g->offset+n)^1];
 }
 for(unsigned n=0;n<3072;n++){
  unsigned group=n/16,tag=(uint16_t)word(ram,owner+group*2);
  if(tag>0xfff0)return 2;
  bases[n]=bfpWord(0xe0000+tag*2+(n%16)*2);
  unsigned offset=(ram[(owner+384+group/2)^1]>>((group&1)?4:0))&15;
  openingColors[n]=bases[n]+(offset<<12);
 }
 for(unsigned n=0;n<2;n++)if((uint16_t)word(ram,slots+n*34)!=65535)return 2;
 WillowNativeTaskRuntime runtime;
 for(unsigned n=0;n<16;n++)runtime.resume[n]=word(ram,control+52+n*2);
 const WillowVisibilityBus bus={nptRead,nptByte,nptWrite,nptPut};
 unsigned dispatches=0,wakes=0,words=0,initialFree=nptRead(0x20c);
 setVideo(nativeRngVideo);
 for(unsigned frame=0;frame<1024;frame++){
  unsigned active=0;
  for(unsigned slot=0x100;slot<512;slot+=32){
   unsigned state=nptByte(slot);if(!state)continue;active++;
   if(state==1||state==2)continue;
   unsigned args[]={slot,0};
   WillowNativeTaskRun result=willowNativeTaskRunOneWithVideo(&runtime,&bus,slot,0,bfpWord,openingRead,openingWrite,0,0,0);
   if(result.unsupported||!result.dispatched){fprintf(stderr,"Opening fade model rejected slot %x pc %x\n",slot,runtime.resume[slot/32]);return 1;}
   objectTaskScatter(expected,&runtime,control,records,globals);
   for(unsigned group=0;group<192;group++){
    uint16_t delta=openingColors[group*16]-bases[group*16];if(delta&0xfff)return 1;
    unsigned at=(owner+384+group/2)^1,shift=(group&1)?4:0;
    expected[at]=(expected[at]&~(15<<shift))|((delta>>12)<<shift);
    for(unsigned n=0;n<16;n++)if((uint16_t)(bases[group*16+n]+delta)!=openingColors[group*16+n])return 1;
   }
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneBootTaskDispatch",2,args)!=0x424f||
      memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved)){
    fprintf(stderr,"Resident opening fade mismatch frame %u slot %x\n",frame,slot);
    for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Fade RAM %x %x/%x\n",n^1,ram[n],expected[n]);break;}return 1;
   }
   dispatches++;words+=3072;
  }
  if(!active){
   if(!nptRead(0x82c0)||nptByte(0xf890)||nptRead(0x20c)!=8||initialFree>=8)return 1;
   setVideo(video);
   printf("Resident opening fades: %u dispatches, %u source wakes, %u exact palette words; all allocated tasks release, dark/busy flags and full unrelated RAM/SRAM match; player slots stay free\n",dispatches,wakes,words);return 0;
  }
  willowArcadeTaskWake(&bus);objectTaskScatter(expected,&runtime,control,records,globals);wakes++;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceTasksWake",0,NULL)==~0u||
     memcmp(expected,ram,sizeof expected)||memcmp(saved,sram,sizeof saved))return 1;
 }
 fputs("Opening fades did not finish within the fixture bound\n",stderr);return 1;
}
