/* Direct compiled writer check, including exact overflow side effects. */
static int nativeObjectWordLeafCheck(uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void))
{
 uint8_t initial[65536],initialSRAM[65536],expected[65536],expectedSRAM[65536];
 memcpy(initial,ram,sizeof initial);memcpy(initialSRAM,sram,sizeof initialSRAM);
 unsigned overflow=symbol(symbols,"debugIntroOverflow"),checks=0;
 unsigned edges[]={0,0x900000,0x900800,0x908000,0x908800,0x910000,0xffffffff};
 unsigned addresses[2200],count=0;
 for(unsigned bank=0;bank<2;bank++)for(unsigned n=0;n<1024;n++)addresses[count++]=0x900000+bank*0x8000+n*2;
 for(unsigned n=0;n<sizeof edges/sizeof edges[0];n++)for(int delta=-2;delta<=2;delta++)addresses[count++]=edges[n]+delta;
 for(unsigned bit=16;bit<32;bit++)for(unsigned bank=0;bank<2;bank++)addresses[count++]=(0x900000+bank*0x8000)^(1u<<bit);
 for(unsigned n=0;n<count;n++){
  uint32_t at=addresses[n];unsigned args[]={at,(n*97+0x5a3c)&65535};
  memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
  memcpy(expected,initial,sizeof expected);memcpy(expectedSRAM,initialSRAM,sizeof expectedSRAM);
  if(n&1){ram[overflow^1]=ram[(overflow+1)^1]=255;expected[overflow^1]=expected[(overflow+1)^1]=255;}
  unsigned valid=!(at&1)&&((at>=0x900000&&at<0x900800)||(at>=0x908000&&at<0x908800));
  if(valid){unsigned offset=at<0x908000?at-0x900000:2048+at-0x908000;expectedSRAM[offset]=args[1]>>8;expectedSRAM[offset+1]=args[1];}
  else{unsigned value=(word(ram,overflow)+1)&65535;expected[overflow^1]=value>>8;expected[(overflow+1)^1]=value;}
  if(nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceObjectVideoWord",2,args)==~0u||
     memcmp(ram,expected,0xf600)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
   fprintf(stderr,"Object word leaf mismatch at %08x\n",at);return 1;
  }
  checks++;
 }
 memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
 printf("Object word leaf: all 2048 bank words plus %u boundary/high-alias cases preserve exact writes, overflow and unrelated RAM/SRAM\n",checks-2048);
 return 0;
}
