static int nativeHideCapture(const char*path)
{
 FILE*in=fopen("build/intro-probe-930.ppm","rb"),*out=fopen(path,"wb");
 if(!in||!out)return 1;
 unsigned char bytes[4096];size_t n;
 while((n=fread(bytes,1,sizeof bytes,in)))if(fwrite(bytes,1,n,out)!=n)return 1;
 int bad=ferror(in);fclose(in);if(fclose(out))bad=1;return bad;
}
/* Terminal bandwidth experiment only. Does not implement source layer order,
 * restore cleared tables, or hide Scroll1 currently represented by sprites. */
static int nativePlaneHideCheck(void*core,unsigned char*ram,unsigned char*vram,
 const char*symbols,void(*runFrame)(void),unsigned(*getReg)(int),unsigned narrow)
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");
 unsigned char*registers=dlsym(core,"reg"),*cram=dlsym(core,"cram");
 if(!setReg||!registers||!cram)return 2;
 unsigned char before[65536],colors[128],originalColors[128],mirrors[5504];
 memcpy(before,vram,sizeof before);memcpy(colors,cram,sizeof colors);memcpy(originalColors,cram,sizeof originalColors);
 unsigned a=symbol(symbols,"planeAAttrs"),b=symbol(symbols,"planeBAttrs");
 memcpy(mirrors,ram+a,2752);memcpy(mirrors+2752,ram+b,2752);
 unsigned char enabled=registers[1]&0x40;if(!enabled)return 1;
 unsigned prefix=0,cpuSAT=symbol(symbols,"willowSAT");
 unsigned char savedSAT[640];
 if(narrow==3){
  unsigned at=symbol(symbols,word(ram,symbol(symbols,"debugHudActive"))?"debugHudSpriteCount":"textSpriteCount");
  prefix=(unsigned short)word(ram,at);
  unsigned count=1;while(count<80 && (before[0xf400+(count-1)*8+2]&127)==count)count++;
  if((before[0xf400+(count-1)*8+2]&127)||prefix>count)return 1;
 }
 unsigned pc=0xf800;
#define HIDE_WORD(v) do{unsigned w=(v);ram[pc++^1]=w>>8;ram[pc++^1]=w;}while(0)
#define HIDE_LONG(v) do{unsigned l=(v);HIDE_WORD(l>>16);HIDE_WORD(l);}while(0)
#define HIDE_CALL(name) do{HIDE_WORD(0x4eb9);HIDE_LONG(fullSymbol(symbols,name));}while(0)
 if(narrow==2){
  /* PAL_getColor is not linked in the ordinary ROM. Use its documented
   * CRAM read sequence directly in the test stub. */
  HIDE_WORD(0x23fc);HIDE_LONG(0x20);HIDE_LONG(0xc00004);
  HIDE_WORD(0x3039);HIDE_LONG(0xc00000);HIDE_WORD(0x0240);HIDE_WORD(0x0eee);
  HIDE_WORD(0x207c);HIDE_LONG(0xfffd20);HIDE_WORD(0x740f);
  unsigned fill=pc;HIDE_WORD(0x30c0);HIDE_WORD(0x51ca);unsigned branch=pc;HIDE_WORD((fill-branch)&65535);
 }
 if(narrow==3){
  HIDE_CALL("sourceBootPaletteInitialize");unsigned prepStop=0xff0000+pc;HIDE_WORD(0x60fe);
  setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
  unsigned prepFrames=0;
  do{runFrame();prepFrames++;}while(getReg(16)!=prepStop&&prepFrames<8);
  if(getReg(16)!=prepStop||(getReg(0)&65535)!=1)return 1;
  printf("Backdrop fixture palette initialization: %u frames, excluded from transfer measurement\n",prepFrames);
  if(nativeHideCapture("build/source-layer-before.ppm"))return 1;
  /* The next CPU list must not determine how the displayed list is hidden. */
  memcpy(savedSAT,ram+cpuSAT,sizeof savedSAT);
  memset(ram+cpuSAT,0,sizeof savedSAT);
  pc=0xf800;
 }
 HIDE_CALL("VDP_waitVSync");
 HIDE_WORD(0x33f9);HIDE_LONG(0xc00008);HIDE_LONG(0xfffd00);
 if(!narrow)for(unsigned plane=0;plane<2;plane++){
  HIDE_WORD(0x2f3c);HIDE_LONG(1);HIDE_WORD(0x2f3c);HIDE_LONG(plane);
  HIDE_CALL("VDP_clearPlane");HIDE_WORD(0x508f);
 }
 if(narrow==1){
  /* ABI-preserved D2/D3/D4/D5 hold row count, plane base, tile index,
   * and plane count. Clear 43 columns in each of the 32 rows. */
  HIDE_WORD(0x263c);HIDE_LONG(0xc000);HIDE_WORD(0x7a01);
  unsigned outer=pc;HIDE_WORD(0x741f);HIDE_WORD(0x7800);
  unsigned inner=pc;
  HIDE_WORD(0x4878);HIDE_WORD(1);HIDE_WORD(0x4878);HIDE_WORD(43);
  HIDE_WORD(0x2f04);HIDE_WORD(0x2f03);HIDE_CALL("VDP_clearTileMap");
  HIDE_WORD(0x4fef);HIDE_WORD(16);HIDE_WORD(0x0644);HIDE_WORD(64);
  HIDE_WORD(0x51ca);unsigned branch=pc;HIDE_WORD((inner-branch)&65535);
  HIDE_WORD(0x0643);HIDE_WORD(0x2000);
  HIDE_WORD(0x51cd);branch=pc;HIDE_WORD((outer-branch)&65535);
 }
 if(narrow==3)HIDE_CALL("sourceBackgroundHideProbe");
 if(narrow==2)for(unsigned palette=2;palette<4;palette++){
  HIDE_WORD(0x4878);HIDE_WORD(1);HIDE_WORD(0x2f3c);HIDE_LONG(0xfffd20);
  HIDE_WORD(0x4878);HIDE_WORD(palette);HIDE_CALL("PAL_setPalette");
  HIDE_WORD(0x4fef);HIDE_WORD(12);
 }
 HIDE_WORD(0x33f9);HIDE_LONG(0xc00008);HIDE_LONG(0xfffd02);
 unsigned stop=0xff0000+pc;HIDE_WORD(0x60fe);
 setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);
 unsigned frames=0;
 do{runFrame();frames++;if((registers[1]&0x40)!=enabled)return 1;}while(getReg(16)!=stop&&frames<8);
 if(getReg(16)!=stop){fprintf(stderr,"Plane clear stalled PC %x\n",getReg(16));return 1;}
 for(unsigned n=0;n<65536;n++){
  unsigned inPlane=(n>=0xc000&&n<0xd000)||(n>=0xe000&&n<0xf000);
  unsigned char expected=inPlane&&narrow<2&&(!narrow||(n&127)<86)?0:before[n];
  if(narrow==3 && prefix && n>=0xf400 && n<0xf408){
   unsigned count=1;while(count<80 && (before[0xf400+(count-1)*8+2]&127)==count)count++;
   expected=n==0xf402 && prefix<count?prefix:0;
  }
  if(vram[n]!=expected){fprintf(stderr,"Plane clear unexpected VRAM %04x\n",n);return 1;}
 }
 if(narrow==2)for(unsigned n=64;n<128;n++)colors[n]=colors[n&1];
 if(narrow==3){
  unsigned char original[6144];FILE*f=fopen("build/boot-palette-original-words.bin","rb");
  if(!f||fread(original,1,sizeof original,f)!=sizeof original)return 2;fclose(f);
  unsigned value=nptVDPColor(willowMotionWord(original+6142));
  if((getReg(0)&65535)!=value)return 1;
  for(unsigned n=64;n<128;n++)colors[n]=(n&1)?value>>8:value&255;
 }
 if(memcmp(colors,cram,sizeof colors)||memcmp(mirrors,ram+a,2752)||memcmp(mirrors+2752,ram+b,2752))return 1;
 unsigned start=((unsigned)ram[0xfd00^1]<<8)|ram[0xfd01^1];
 unsigned end=((unsigned)ram[0xfd02^1]<<8)|ram[0xfd03^1];
 printf("Display-enabled background operation: %s; expected VRAM/CRAM changes verified, object SAT entries/patterns and CPU mirrors preserved; %u observed frames including VBlank wait, HV %04x -> %04x\n",narrow==3?"PAL2/PAL3 set from retained CPS pen BFF; PAL0/PAL1 intact":narrow==2?"PAL2/PAL3 set to palette-zero color while PAL0/PAL1 stay intact":narrow?"43 columns per row on both planes cleared":"both 4096-byte tables cleared",frames,start,end);
 if(narrow==3){runFrame();if(nativeHideCapture("build/source-layer-hidden.ppm"))return 1;}
 if(narrow>=2 && ((start>>8)<224 || (end>>8)<(start>>8) || frames>2))return 1;
 if(narrow>=2){
  /* The current intro's logical background palettes live in ROM. Restore
   * from that owner, without allocating a saved-palette buffer in the game.
   * This is deliberately not a generic restoration source for later stages. */
  pc=0xf800;HIDE_CALL("VDP_waitVSync");
  HIDE_WORD(0x33f9);HIDE_LONG(0xc00008);HIDE_LONG(0xfffd00);
  for(unsigned palette=2;palette<4;palette++){
   HIDE_WORD(0x4878);HIDE_WORD(1);HIDE_WORD(0x2f3c);
   HIDE_LONG(fullSymbol(symbols,"intro_palettes")+(palette-2)*32);
   HIDE_WORD(0x4878);HIDE_WORD(palette);HIDE_CALL("PAL_setPalette");
   HIDE_WORD(0x4fef);HIDE_WORD(12);
  }
  if(narrow==3){
   for(unsigned n=0;n<sizeof savedSAT;n++)if(ram[cpuSAT+n])return 1;
   memcpy(ram+cpuSAT,savedSAT,sizeof savedSAT);
   HIDE_CALL("willowSATRestoreHead");
  }
  HIDE_WORD(0x33f9);HIDE_LONG(0xc00008);HIDE_LONG(0xfffd02);
  stop=0xff0000+pc;HIDE_WORD(0x60fe);
  setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);frames=0;
  do{runFrame();frames++;if((registers[1]&0x40)!=enabled)return 1;}while(getReg(16)!=stop&&frames<8);
  if(getReg(16)!=stop||memcmp(cram,originalColors,sizeof originalColors)||memcmp(vram,before,sizeof before)||
     memcmp(mirrors,ram+a,2752)||memcmp(mirrors+2752,ram+b,2752))return 1;
  start=((unsigned)ram[0xfd00^1]<<8)|ram[0xfd01^1];end=((unsigned)ram[0xfd02^1]<<8)|ram[0xfd03^1];
  printf("Intro palette restoration: exact original CRAM restored from ROM with no game heap allocation; VRAM/SAT/mirrors unchanged; %u frames including wait, HV %04x -> %04x\n",frames,start,end);
  if((start>>8)<224||(end>>8)<224||frames>2)return 1;
  if(narrow==3){runFrame();if(nativeHideCapture("build/source-layer-restored.ppm"))return 1;}
 }
 if(narrow==3){
  unsigned owner=symbol(symbols,"debugPaletteOwner"),tagAt=owner+191*2;
  unsigned char savedTag[2]={ram[tagAt^1],ram[(tagAt+1)^1]},unchanged[0xf600];
  const unsigned badTags[]={0xffff,0x9000};
  for(unsigned n=0;n<2;n++){
   ram[tagAt^1]=badTags[n]>>8;ram[(tagAt+1)^1]=badTags[n];
   memcpy(unchanged,ram,sizeof unchanged);
   pc=0xf800;HIDE_CALL("sourceBackgroundHideProbe");stop=0xff0000+pc;HIDE_WORD(0x60fe);
   setReg(17,0x2700);setReg(15,0xffff00);setReg(16,0xfff800);frames=0;
   do{runFrame();frames++;}while(getReg(16)!=stop&&frames<8);
   if(getReg(16)!=stop||(getReg(0)&65535)!=0xffff||memcmp(unchanged,ram,sizeof unchanged)||
      memcmp(cram,originalColors,sizeof originalColors)||memcmp(vram,before,sizeof before))return 1;
  }
  ram[tagAt^1]=savedTag[0];ram[(tagAt+1)^1]=savedTag[1];
  puts("Source backdrop transfer: unowned and missing ROM reject before any CRAM/VRAM or non-stack RAM mutation");
 }
 if(narrow==3)puts("Displayed SAT prefix survives replacement of the entire next CPU list; CPU list is preserved by hide");
 puts("Terminal feasibility measurement only: generic stage/fade restoration and source-phase alignment remain unimplemented; prefix filtering is checked only for this published list");
#undef HIDE_CALL
#undef HIDE_LONG
#undef HIDE_WORD
 return 0;
}
