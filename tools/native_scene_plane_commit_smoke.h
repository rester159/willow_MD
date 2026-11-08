static void sceneCommitPut(unsigned char*r,unsigned at,unsigned value)
{r[at^1]=value>>8;r[(at+1)^1]=value;}
/* Run after the real boot clear. Prepare outside VBlank, then publish and
 * retire each plane. Expected ownership is independent per-cell subtraction. */
static int nativeScenePlaneCommitCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int),unsigned combined)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 unsigned char*vram=dlsym(core,"vram"),*cram=dlsym(core,"cram"),*regs=dlsym(core,"reg");
 if(!setReg||!setVideo||!memory||!vram||!cram||!regs)return 2;
 unsigned char*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 unsigned char expected[0xf600],beforeVRAM[65536],colors[128],savedSRAM[65536];
 unsigned refs=symbol(symbols,"refsBySlot"),freeAt=symbol(symbols,"freeTiles");
 unsigned streams=longword(ram,symbol(symbols,"columnStreams"))&65535;
 unsigned cleared=symbol(symbols,"sourceSceneClearedPlanes"),prepared=symbol(symbols,"sourceScenePreparedPlanes");
 unsigned mirrors[]={symbol(symbols,"planeAAttrs"),symbol(symbols,"planeBAttrs")};
 if(combined){
  /* Add overlapping non-plane owners to the terminal fixture. Their pixels
   * are not published; this checks that reconstruction retains multiplicity
   * and a pin at the last cache slot, independently of current HUD owners. */
  unsigned overlay=symbol(symbols,"overlayLoaded"),bank=symbol(symbols,"debugBankCount");
  if(word(ram,overlay)||word(ram,bank))return 1;
  sceneCommitPut(ram,overlay,3);sceneCommitPut(ram,symbol(symbols,"overlayFirstSlot"),500);
  sceneCommitPut(ram,bank,4);
  const unsigned slots[]={500,501,502,500,500,502,1295};
  for(unsigned n=0;n<7;n++){
   unsigned slot=slots[n],at=refs+slot*2;
   sceneCommitPut(ram,at,(unsigned short)word(ram,at)+1);
   at=freeAt+(slot/16)*2;
   unsigned bits=(unsigned short)word(ram,at)&~(1u<<(slot%16));sceneCommitPut(ram,at,bits);
   if(!bits){at=freeAt+162+(slot/256)*2;sceneCommitPut(ram,at,(unsigned short)word(ram,at)&~(1u<<((slot/16)%16)));}
   if(n>=3)sceneCommitPut(ram,symbol(symbols,"debugBankTiles")+(n-3)*2,slot);
  }
 }
 unsigned released=0;
 for(unsigned n=0;n<32;n++)if(vram[n])return 1;
 setVideo(nativeRngVideo);
 const char*commit=combined?"sourceScenePlanesCommitProbe":"sourceScenePlaneCommitProbe";
 for(unsigned pass=0;pass<(combined?1u:2u);pass++){
  unsigned first=combined?0:pass,last=combined?1:pass,layer=first;
  unsigned args[]={layer};memcpy(expected,ram,sizeof expected);
  /* An unprepared commit must not touch either memory or the VDP. */
  memcpy(beforeVRAM,vram,sizeof beforeVRAM);memcpy(savedSRAM,sram,sizeof savedSRAM);
  memcpy(colors,cram,sizeof colors);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,commit,combined?0:1,args)||
     memcmp(expected,ram,sizeof expected)||memcmp(beforeVRAM,vram,sizeof beforeVRAM)||memcmp(savedSRAM,sram,sizeof savedSRAM)){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
  for(layer=first;layer<=last;layer++){
  args[0]=layer;
  expected[prepared^1]|=1u<<layer;
  unsigned preparation=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceScenePlanePrepareProbe",1,args);
  if(preparation!=1){unsigned rows=symbol(symbols,"rowOwned")+layer*4;fprintf(stderr,"Prepare result %u cleared %x row owners %02x %02x %02x %02x\n",preparation,ram[cleared^1],ram[rows^1],ram[(rows+1)^1],ram[(rows+2)^1],ram[(rows+3)^1]);}
  if(preparation!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(beforeVRAM,vram,sizeof beforeVRAM)||memcmp(savedSRAM,sram,sizeof savedSRAM)){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
  if(combined&&layer==0&&(nativeSATCall(ram,symbols,setReg,getReg,runFrame,commit,0,NULL)||
     memcmp(expected,ram,sizeof expected)||memcmp(beforeVRAM,vram,sizeof beforeVRAM)||memcmp(savedSRAM,sram,sizeof savedSRAM)))return 1;
  }
  for(layer=first;layer<=last;layer++){
  unsigned stream=streams+layer*132,mirror=mirrors[layer];
  for(unsigned slot=0;slot<43;slot++){
   int column=(short)word(expected,stream+slot*2);unsigned rows=expected[(stream+86+slot)^1];
   if(column!=-32768)for(unsigned row=0;row<rows;row++){
    unsigned tile=(unsigned short)word(expected,mirror+(row*43+slot)*2)&2047;
    if(tile<16||tile>=16+1296){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
    unsigned at=refs+(tile-16)*2,value=(unsigned short)word(expected,at);
    if(!value){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}sceneCommitPut(expected,at,value-1);released++;
   }
   sceneCommitPut(expected,stream+slot*2,0x8000);expected[(stream+86+slot)^1]=0;
  }
  unsigned rowOwners=symbol(symbols,"rowOwned")+layer*4;
  for(unsigned row=0;row<32;row++)if(expected[(rowOwners+row/8)^1]&(1u<<(row%8)))
   for(unsigned x=0;x<40;x++){
    unsigned tile=(unsigned short)word(expected,mirror+(row*43+x)*2)&2047;
    if(tile<16||tile>=16+1296)return 1;
    unsigned at=refs+(tile-16)*2,value=(unsigned short)word(expected,at);
    if(!value)return 1;sceneCommitPut(expected,at,value-1);released++;
   }
  for(unsigned n=0;n<4;n++)expected[(rowOwners+n)^1]=0;
  for(unsigned n=0;n<2752;n++)expected[(mirror+n)^1]=0;
  for(unsigned group=0;group<6;group++)sceneCommitPut(expected,freeAt+162+group*2,0);
  for(unsigned n=0;n<81;n++){
   unsigned bits=0;for(unsigned bit=0;bit<16;bit++)if(!word(expected,refs+(n*16+bit)*2))bits|=1u<<bit;
   sceneCommitPut(expected,freeAt+n*2,bits);
   if(bits){unsigned at=freeAt+162+(n/16)*2;sceneCommitPut(expected,at,(unsigned short)word(expected,at)|(1u<<(n%16)));}
  }
  sceneCommitPut(expected,symbol(symbols,"pendingRow")+layer*2,65535);
  sceneCommitPut(expected,symbol(symbols,"pendingColumns")+layer*2,0);
  unsigned ready=symbol(symbols,"debugColumnReady");sceneCommitPut(expected,ready,(unsigned short)word(expected,ready)&~(1u<<layer));
  expected[cleared^1]&=~(1u<<layer);expected[prepared^1]&=~(1u<<layer);
  }
  layer=first;args[0]=layer;
  if(combined){
   expected[cleared^1]|=4;
   sceneCommitPut(expected,symbol(symbols,"bgb_addr"),0xc000);
   expected[(symbol(symbols,"regValues")+4)^1]=6;
  }
  unsigned pc=0xf800;
#define SCP_W(v) do{unsigned q=(v);ram[pc++^1]=q>>8;ram[pc++^1]=q;}while(0)
#define SCP_L(v) do{unsigned ql=(v);SCP_W(ql>>16);SCP_W(ql);}while(0)
  /* Poll the next blank directly so SDK CPU-load bookkeeping is not part
   * of the renderer mutation being compared. Interrupts stay masked. */
  unsigned wait=pc;SCP_W(0x3039);SCP_L(0xc00004);SCP_W(0x0240);SCP_W(8);
  SCP_W(0x6600);unsigned branch=pc;SCP_W((wait-branch)&65535);
  wait=pc;SCP_W(0x3039);SCP_L(0xc00004);SCP_W(0x0240);SCP_W(8);
  SCP_W(0x6700);branch=pc;SCP_W((wait-branch)&65535);
  SCP_W(0x33f9);SCP_L(0xc00008);SCP_L(0xfffd00);
  if(!combined){SCP_W(0x2f3c);SCP_L(layer);}
  SCP_W(0x4eb9);SCP_L(fullSymbol(symbols,commit));
  if(!combined)SCP_W(0x588f);
  unsigned stop=0xff0000+pc;SCP_W(0x60fe);
  unsigned enabled=regs[1]&0x40,increment=regs[15];
  sceneCommitPut(ram,0xfd00,65535);sram[0x1804]=sram[0x1805]=255;
  setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
  unsigned frames=0,startFrame=0,finishFrame=0;
  do{runFrame();frames++;
   if(!startFrame&&(unsigned short)word(ram,0xfd00)!=65535)startFrame=frames;
   if(!finishFrame&&(sram[0x1804]!=255||sram[0x1805]!=255))finishFrame=frames;
  }while(getReg(16)!=stop&&frames<8);
  if(getReg(16)!=stop||(getReg(0)&65535)!=1||memcmp(expected,ram,sizeof expected)){
   for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Plane %u RAM mismatch %04x %02x/%02x\n",layer,n^1,ram[n],expected[n]);break;}{fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
  }
  unsigned start=(unsigned short)word(ram,0xfd00),end=(sram[0x1802]<<8)|sram[0x1803];
  savedSRAM[0x1802]=sram[0x1802];savedSRAM[0x1803]=sram[0x1803];
  savedSRAM[0x1804]=sram[0x1804];savedSRAM[0x1805]=sram[0x1805];
  if(memcmp(savedSRAM,sram,sizeof savedSRAM)||memcmp(colors,cram,sizeof colors)||
     (regs[1]&0x40)!=enabled||regs[15]!=increment){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
  unsigned begin=layer?0xe000:0xc000;
  for(unsigned n=0;n<65536;n++)if(vram[n]!=(n>=begin&&n<begin+4096?0:beforeVRAM[n])){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
  if(combined&&regs[4]!=6)return 1;
  printf("Resident plane %u clear (2 means both): hardware fill HV %04x -> %04x, %u observed frames including wait and CPU retirement; other plane, SAT, patterns, CRAM and display enable preserved\n",combined?2:layer,start,end,frames);
  printf("Plane %u active commit markers: start observed frame %u HV %04x, retirement end frame %u HV %02x%02x (wait excluded from start marker)\n",combined?2:layer,startFrame,start,finishFrame,sram[0x1804],sram[0x1805]);
  if(!startFrame||startFrame!=finishFrame||frames>2){fprintf(stderr,"Plane commit exceeded one active emulator frame\n");return 1;}
  if((start>>8)<224||(end>>8)<(start>>8)){fprintf(stderr,"Scene plane fixture failed at line %d\n",__LINE__);return 1;}
  memcpy(beforeVRAM,vram,sizeof beforeVRAM);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,commit,combined?0:1,args)||
     memcmp(expected,ram,sizeof expected)||memcmp(beforeVRAM,vram,sizeof beforeVRAM)||memcmp(savedSRAM,sram,sizeof savedSRAM))return 1;
#undef SCP_L
#undef SCP_W
 }
 setVideo(video);
 runFrame();if(nativeHideCapture(combined?"build/scene-combined-cleared.ppm":"build/scene-plane-cleared.ppm"))return 1;
 if(combined){
  setVideo(nativeRngVideo);
  memcpy(expected,ram,sizeof expected);memcpy(beforeVRAM,vram,sizeof beforeVRAM);
  memcpy(savedSRAM,sram,sizeof savedSRAM);
  expected[cleared^1]&=~4;sceneCommitPut(expected,symbol(symbols,"bgb_addr"),0xe000);
  expected[(symbol(symbols,"regValues")+4)^1]=7;
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceScenePlanesSeparateProbe",0,NULL)!=1||
     memcmp(expected,ram,sizeof expected)||memcmp(savedSRAM,sram,sizeof savedSRAM)||
     memcmp(colors,cram,sizeof colors)||regs[4]!=7)return 1;
  for(unsigned n=0;n<65536;n++)if(vram[n]!=(n>=0xe000&&n<0xf000?0:beforeVRAM[n]))return 1;
  memcpy(beforeVRAM,vram,sizeof beforeVRAM);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceScenePlanesSeparateProbe",0,NULL)||
     memcmp(expected,ram,sizeof expected)||memcmp(savedSRAM,sram,sizeof savedSRAM)||memcmp(beforeVRAM,vram,sizeof beforeVRAM))return 1;
  setVideo(video);runFrame();if(nativeHideCapture("build/scene-combined-separated.ppm"))return 1;
  puts("Combined clear: both layers share the blank table, then B's inactive table is cleared before its original address is restored; ownership, SAT and palette preserved throughout");
 }
 printf("Resident scene plane ownership: %u cell references released once; complete refs/free index, empty streams/mirrors and surviving owners verified\n",released);
 return 0;
}
