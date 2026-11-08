/* Differentially exercise the service-local bus without invoking gameplay.
 * Test data replaces only actor payload bytes; the exact entry state is restored. */
static int nativeDemoActorBusCheck(void*core,uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*runFrame)(void),unsigned(*getReg)(int))
{
 void(*setReg)(int,unsigned)=dlsym(core,"m68k_set_reg");if(!setReg)return 2;
 uint8_t initial[65536],initialSRAM[65536],seed[65536],seedSRAM[65536],expected[65536],expectedSRAM[65536];
 memcpy(initial,ram,sizeof initial);memcpy(initialSRAM,sram,sizeof initialSRAM);
 for(unsigned at=0x98c6;at<0xefc6;at++)sram[at]=(at*73+(at>>8)+0x29)&255;
 memcpy(seed,ram,sizeof seed);memcpy(seedSRAM,sram,sizeof seedSRAM);
 const char*old[]={"sourceOpeningBusByte","sourceOpeningBusWord","sourceOpeningBusPutByte","sourceOpeningBusPutWord","sourceOpeningResolve"};
 const char*fast[]={"sourceDemoBusByte","sourceDemoBusWord","sourceDemoBusPutByte","sourceDemoBusPutWord","sourceDemoResolve"};
 unsigned checks=0;
 unsigned fixed=getenv("WILLOW_AUDIT_DIRECT_FIXED_RECORDS")!=NULL;
 if(fixed){
  unsigned total=0;
  for(unsigned page=0;page<256;page++){
   unsigned wanted=0;
   for(unsigned at=page*256;at<(page+1)*256;at++)wanted+=(at>=0x9846&&at<0x98c6)||(at>=0x8502&&at<0x9102)||(at>=0xf042&&at<0xf442);
   unsigned got=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoFixedPointerProbe",1,&page);
   if(got!=wanted||memcmp(ram,seed,0xf600)||memcmp(sram,seedSRAM,sizeof seedSRAM))return 1;
   total+=got;
  }
  if(total!=4224)return 1;
  puts("Direct fixed records: all 65536 source addresses checked; 4224 exact legacy pointers, fallback holes and full unrelated RAM/SRAM preserved");
 }
 for(unsigned group=0;group<9+(fixed?3:0);group++){
  const unsigned fixedStarts[]={0x9846,0x8502,0xf042},fixedSizes[]={128,3072,1024};
  unsigned type=(group+1)*2,base,end;
  if(group<9){const WillowActorPoolSpec*spec=willowActorPoolSpecification(type);base=spec->sourceBase;end=base+spec->capacity*spec->stride;}
  else{base=fixedStarts[group-9];end=base+fixedSizes[group-9];}
  unsigned addresses[]={base-2,base-1,base,base+1,base+2,end-2,end-1,end};
  for(unsigned n=0;n<8;n++)for(unsigned op=0;op<5;op++){
   unsigned args[]={addresses[n],0xa53c^(n*71)},count=(op==2||op==3)?2:1;
   memcpy(ram,seed,sizeof seed);memcpy(sram,seedSRAM,sizeof seedSRAM);
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,old[op],count,args)==~0u)return 1;
   unsigned result=getReg(0);memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
   memcpy(ram,seed,sizeof seed);memcpy(sram,seedSRAM,sizeof seedSRAM);
   if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,fast[op],count,args)==~0u)return 1;
   unsigned mask=op==0?255:op==1?65535:op==4?~0u:0;
   if(((result^getReg(0))&mask)||memcmp(ram,expected,0xf600)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
    fprintf(stderr,"Direct actor bus mismatch class %u at %04x op %u\n",type,addresses[n],op);return 1;
   }
   checks++;
  }
 }
 memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
 /* Each malformed descriptor must reject before any update state changes. */
 const unsigned order[]={4,0,1,5,6,2,7,3};unsigned rejected=0;
 for(unsigned i=0;i<9;i++)for(unsigned test=0;test<6;test++){
  if(i==8&&(test==1||test==2))continue; /* Class 18 uses its legacy binding. */
  unsigned pool=i==8?symbol(symbols,"tileIndex")+4648:symbol(symbols,"debugActorPools")+order[i]*12;
  memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
  if(test<4){
   unsigned offset=test==3?4:0,value=test==1?longword(ram,pool)+2:test==2?0xff0000|(longword(ram,pool)&65535):0;
   for(unsigned n=0;n<4;n++)ram[(pool+offset+n)^1]=value>>(24-n*8);
  }else ram[(pool+(test==4?8:9))^1]=0;
  memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoUpdateProbe",0,NULL)||
     memcmp(ram,expected,0xf600)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
   fprintf(stderr,"Direct actor binding admitted/mutated class %u test %u\n",(i+1)*2,test);return 1;
  }
  rejected++;
 }
 memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
 unsigned cache=symbol(symbols,"lookupBuilt");ram[cache^1]=0;ram[(cache+1)^1]=0;
 memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
 if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoUpdateProbe",0,NULL)||
    memcmp(ram,expected,0xf600)||memcmp(sram,expectedSRAM,sizeof expectedSRAM))return 1;
 memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
 printf("Direct actor bus: %u boundary/odd-word/read/write/resolve comparisons match legacy with full unrelated memory preserved; %u invalid bindings plus unavailable class-18 storage reject before mutation\n",checks,rejected);
 return 0;
}
