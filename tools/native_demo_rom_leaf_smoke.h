/* Compare every retained word in the compiled 68000 leaf and C reader, then
 * compare boundary/hole/source-RAM calls including exact fault/caller state. */
static int nativeDemoROMLeafCheck(uint8_t*ram,uint8_t*sram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void))
{
 uint8_t initial[65536],initialSRAM[65536],expected[65536],expectedSRAM[65536];
 memcpy(initial,ram,sizeof initial);memcpy(initialSRAM,sram,sizeof initialSRAM);
 unsigned arg=65535,spans=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoROMLeafProbe",1,&arg);
 arg=65534;unsigned words=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoROMLeafProbe",1,&arg);
 if(!spans||spans>1024||!words||words>=65534)return 1;
 for(unsigned n=0;n<spans;n++)if(nativeSATCallBounded(64,NULL,ram,symbols,setReg,getReg,runFrame,"sourceDemoROMLeafProbe",1,&n)!=1||
    memcmp(initial,ram,0xf600)||memcmp(initialSRAM,sram,sizeof initialSRAM))return 1;
 const unsigned edges[]={0x3c7b6,0x3cc4a,0x27318,0x27320,0x27716,0xea260,0xea620,0x36cb4,0x36cb8,0x3b9b2,0x3b9b6,0,0x100000,0xffffffff,0xff9846,0xff98c6,0xfff042,0xff8004};
 unsigned checks=0;
 for(unsigned n=0;n<sizeof edges/sizeof edges[0];n++)for(int delta=-2;delta<=2;delta++){
  unsigned at=edges[n]+delta;
  memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
  unsigned wanted=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoWordFallback",1,&at);
  if(wanted==~0u)return 1;
  memcpy(expected,ram,sizeof expected);memcpy(expectedSRAM,sram,sizeof expectedSRAM);
  memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
  unsigned got=nativeSATCall(ram,symbols,setReg,getReg,runFrame,"sourceDemoWord",1,&at);
  if(got!=wanted||memcmp(ram,expected,0xf600)||memcmp(sram,expectedSRAM,sizeof expectedSRAM)){
   fprintf(stderr,"Native ROM leaf mismatch at %08x: %x/%x\n",at,got,wanted);return 1;
  }
  checks++;
 }
 memcpy(ram,initial,sizeof initial);memcpy(sram,initialSRAM,sizeof initialSRAM);
 printf("Native ROM leaf: %u retained words in %u spans match C/storage; %u boundary, odd, hole, overflow-address and source-RAM calls match C including fault/caller state; unrelated RAM/SRAM preserved\n",words,spans,checks);
 return 0;
}
