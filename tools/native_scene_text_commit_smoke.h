static int nativeSceneTextCommitCheck(void*core,unsigned char*ram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 void(*setVideo)(retro_video_refresh_t)=dlsym(core,"retro_set_video_refresh");
 void*(*memory)(unsigned)=dlsym(core,"retro_get_memory_data");
 unsigned char*vram=dlsym(core,"vram"),*cram=dlsym(core,"cram"),*regs=dlsym(core,"reg");
 if(!setReg||!setVideo||!memory||!vram||!cram||!regs)return 2;
 unsigned char*sram=memory(RETRO_MEMORY_SAVE_RAM);if(!sram)return 2;
 unsigned char expected[0xf600],beforeVRAM[65536],colors[128],savedSRAM[65536];
 unsigned flags=symbol(symbols,"sourceSceneClearedPlanes"),sat=symbol(symbols,"willowSAT");
 unsigned sprites=symbol(symbols,"debugHudSprites"),hudCount=(unsigned short)word(ram,symbol(symbols,"debugHudSpriteCount"));
 unsigned prefix=ram[symbol(symbols,"satPublishedPrefix")^1],count=ram[symbol(symbols,"satPublishedCount")^1];
 unsigned refs=symbol(symbols,"refsBySlot"),freeAt=symbol(symbols,"freeTiles");
 if(!prefix||prefix!=hudCount||count<=prefix||count>80)return 2;
 setVideo(nativeRngVideo);
#define TEXT_CHECK(condition) do{if(!(condition)){fprintf(stderr,"Text publication failed at line %d\n",__LINE__);return 1;}}while(0)
 /* An actual queued SAT must block publication, not be discarded. */
 unsigned args[]={count,prefix,2};
 TEXT_CHECK(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"willowSATUpload",3,args)!=~0u);
 TEXT_CHECK(word(ram,symbol(symbols,"queueIndex"))!=0);
 memcpy(expected,ram,sizeof expected);memcpy(beforeVRAM,vram,sizeof beforeVRAM);
 memcpy(savedSRAM,sram,sizeof savedSRAM);memcpy(colors,cram,sizeof colors);
 TEXT_CHECK(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneTextCommitProbe",0,NULL)==0);
 TEXT_CHECK(!memcmp(expected,ram,sizeof expected)&&!memcmp(beforeVRAM,vram,sizeof beforeVRAM)&&!memcmp(savedSRAM,sram,sizeof savedSRAM));
 TEXT_CHECK(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"DMA_flushQueue",0,NULL)!=~0u);
 TEXT_CHECK(!word(ram,symbol(symbols,"queueIndex")));
 /* A different next CPU prefix cannot determine the displayed head. */
 for(unsigned n=0;n<prefix*8;n++)ram[(sat+n)^1]=0;
 memcpy(expected,ram,sizeof expected);memcpy(beforeVRAM,vram,sizeof beforeVRAM);
 memcpy(savedSRAM,sram,sizeof savedSRAM);
 ram[flags^1]&=~8;expected[flags^1]&=~8;
 TEXT_CHECK(!nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneTextCommitProbe",0,NULL));
 TEXT_CHECK(!memcmp(expected,ram,sizeof expected)&&!memcmp(beforeVRAM,vram,sizeof beforeVRAM)&&!memcmp(savedSRAM,sram,sizeof savedSRAM));
 ram[flags^1]|=8; /* restore the original clear token for the positive case */
 unsigned released=0,ids=symbol(symbols,"ids"),index=symbol(symbols,"tileIndex");
 TEXT_CHECK(word(ram,symbol(symbols,"lookupActive"))==1);
 for(unsigned n=0;n<hudCount;n++){
  unsigned layout=(unsigned short)word(expected,sprites+n*4),tile=(unsigned short)word(expected,sprites+n*4+2);
  unsigned size=(((layout>>11)&3)+1)*(((layout>>13)&3)+1);
  if(tile>=16&&tile<16+1296)for(unsigned i=0;i<size;i++){
   unsigned at=refs+(tile+i-16)*2,value=(unsigned short)word(expected,at);
   TEXT_CHECK(value);sceneCommitPut(expected,at,value-1);released++;
   /* Remove the retiring slot by identity across all buckets, independently
    * of the runtime key-based removal. Preserve all collision-chain peers. */
   unsigned slot=tile+i-16,found=0;
   for(unsigned bucket=0;bucket<512;bucket++){
    unsigned link=index+bucket*2,steps=0;
    while((unsigned short)word(expected,link)!=65535){
     unsigned peer=(unsigned short)word(expected,link);
     TEXT_CHECK(peer<1296&&steps++<1296);
     if(peer==slot){
      sceneCommitPut(expected,link,(unsigned short)word(expected,index+1024+peer*2));
      sceneCommitPut(expected,index+1024+peer*2,65535);found++;break;
     }
     link=index+1024+peer*2;
    }
   }
   TEXT_CHECK(found==1);sceneCommitPut(expected,ids+slot*2,65535);
  }
  sceneCommitPut(expected,sprites+n*4+2,0);
 }
 for(unsigned g=0;g<6;g++)sceneCommitPut(expected,freeAt+162+g*2,0);
 for(unsigned w=0;w<81;w++){
  unsigned bits=0;for(unsigned b=0;b<16;b++)if(!word(expected,refs+(w*16+b)*2))bits|=1u<<b;
  sceneCommitPut(expected,freeAt+w*2,bits);
  if(bits){unsigned at=freeAt+162+(w/16)*2;sceneCommitPut(expected,at,(unsigned short)word(expected,at)|(1u<<(w%16)));}
 }
 unsigned hud=symbol(symbols,"debugHudSurface"),changed=symbol(symbols,"hudSpriteChanged");
 for(unsigned n=0;n<36;n++)expected[(hud+1152+n)^1]=0;
 for(unsigned n=0;n<5;n++)expected[(changed+n)^1]=255;
 expected[symbol(symbols,"hudSpritesDirty")^1]=1;
 for(unsigned n=0;n<10;n++)expected[(symbol(symbols,"textPresent")+n)^1]=0;
 expected[symbol(symbols,"textDirty")^1]=0;sceneCommitPut(expected,symbol(symbols,"textSpriteCount"),0);
 unsigned pc=0xf800;
#define ST_W(v) do{unsigned q=(v);ram[pc++^1]=q>>8;ram[pc++^1]=q;}while(0)
#define ST_L(v) do{unsigned ql=(v);ST_W(ql>>16);ST_W(ql);}while(0)
 unsigned wait=pc;ST_W(0x3039);ST_L(0xc00004);ST_W(0x0240);ST_W(8);
 ST_W(0x6600);unsigned branch=pc;ST_W((wait-branch)&65535);
 wait=pc;ST_W(0x3039);ST_L(0xc00004);ST_W(0x0240);ST_W(8);
 ST_W(0x6700);branch=pc;ST_W((wait-branch)&65535);
 ST_W(0x33f9);ST_L(0xc00008);ST_L(0xfffd00);
 ST_W(0x4eb9);ST_L(fullSymbol(symbols,"sourceSceneTextCommitProbe"));
 unsigned stop=0xff0000+pc;ST_W(0x60fe);
 unsigned enabled=regs[1]&0x40,increment=regs[15];
 sceneCommitPut(ram,0xfd00,65535);sram[0x1804]=sram[0x1805]=255;
 setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
 unsigned frames=0,startFrame=0,finishFrame=0;
 do{runFrame();frames++;
  if(!startFrame&&(unsigned short)word(ram,0xfd00)!=65535)startFrame=frames;
  if(!finishFrame&&(sram[0x1804]!=255||sram[0x1805]!=255))finishFrame=frames;
 }while(getReg(16)!=stop&&frames<8);
 TEXT_CHECK(getReg(16)==stop&&(getReg(0)&65535)==1);
 if(memcmp(expected,ram,sizeof expected)){
  for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){fprintf(stderr,"Text RAM mismatch %04x %02x/%02x\n",n^1,ram[n],expected[n]);break;}return 1;
 }
 for(unsigned n=0x1802;n<0x1806;n++)savedSRAM[n]=sram[n];
 TEXT_CHECK(!memcmp(savedSRAM,sram,sizeof savedSRAM)&&!memcmp(colors,cram,sizeof colors)&&(regs[1]&0x40)==enabled&&regs[15]==increment);
 for(unsigned n=0;n<65536;n++){
  unsigned value=beforeVRAM[n];
  if(n>=0xf400&&n<0xf408)value=n==0xf402?prefix:0;
  TEXT_CHECK(vram[n]==value);
 }
 unsigned start=(unsigned short)word(ram,0xfd00),published=(sram[0x1802]<<8)|sram[0x1803];
 TEXT_CHECK((start>>8)>=224&&(published>>8)>=(start>>8)&&startFrame==finishFrame&&frames<=2);
 memcpy(beforeVRAM,vram,sizeof beforeVRAM);
 TEXT_CHECK(!nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneTextCommitProbe",0,NULL));
 TEXT_CHECK(!memcmp(expected,ram,sizeof expected)&&!memcmp(beforeVRAM,vram,sizeof beforeVRAM)&&!memcmp(savedSRAM,sram,sizeof savedSRAM));
 setVideo(video);runFrame();TEXT_CHECK(!nativeHideCapture("build/scene-text-cleared.ppm"));setVideo(nativeRngVideo);
 /* Re-emitting the retained HUD geometry must produce transparent sprites,
  * so a subsequent fresh SAT upload cannot resurrect the cleared text. */
 for(unsigned n=0;n<hudCount;n++){
  unsigned layout=(unsigned short)word(expected,sprites+n*4),at=sat+n*8;
  sceneCommitPut(expected,at,0);
  expected[(at+2)^1]=((layout>>11)&3)*4+((layout>>13)&3);expected[(at+3)^1]=n+1;
  sceneCommitPut(expected,at+4,0x8000);sceneCommitPut(expected,at+6,(layout&63)*8+128);
  expected[(changed+n/8)^1]&=~(1u<<(n%8));
 }
 expected[symbol(symbols,"hudSpritesDirty")^1]=0;
 TEXT_CHECK(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceSceneHudEmitProbe",0,NULL)==hudCount);
 TEXT_CHECK(!memcmp(expected,ram,sizeof expected)&&!memcmp(beforeVRAM,vram,sizeof beforeVRAM));
 args[2]=1;TEXT_CHECK(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"willowSATUpload",3,args)!=~0u);
 memcpy(beforeVRAM+0xf400,ram+sat,count*8);
 TEXT_CHECK(!memcmp(beforeVRAM,vram,sizeof beforeVRAM)&&!memcmp(expected,ram,sizeof expected)&&!memcmp(colors,cram,sizeof colors));
 setVideo(video);runFrame();TEXT_CHECK(!nativeHideCapture("build/scene-text-republished.ppm"));
 printf("Resident Scroll1 publication: %u displayed text sprites hidden, %u cached glyph references released; objects, palette, source words and HUD geometry/index preserved; HV %04x -> %04x, active start/end frame %u/%u; queued DMA and duplicate/no-clear calls reject, fresh SAT stays blank\n",prefix,released,start,published,startFrame,finishFrame);
#undef ST_L
#undef ST_W
#undef TEXT_CHECK
 return 0;
}
