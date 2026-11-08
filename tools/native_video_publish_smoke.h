/* Compiled native source-bus integration; host arithmetic is independently
 * checked against original instructions by test_arcade_video_publish. */
#include "../src/willow_arcade_video_publish.h"
static unsigned char nvpSource[65536],nvpHardware[256];
static uint16_t nvpWord(uint16_t at){return willowMotionWord(nvpSource+at);}
static uint8_t nvpByte(uint16_t at){return nvpSource[at];}
static void nvpPutWord(uint16_t at,uint16_t value){willowMotionPutWord(nvpSource+at,value);}
static void nvpPutByte(uint16_t at,uint8_t value){nvpSource[at]=value;}
static uint8_t nvpInput(uint16_t at){return nvpHardware[at];}
static void nvpOutByte(uint16_t at,uint8_t value){nvpHardware[at&255]=value;}
static void nvpOutWord(uint16_t at,uint16_t value){willowMotionPutWord(nvpHardware+(at&255),value);}
static int nativeVideoPublishCheck(unsigned char*ram,const char*symbols,
 void(*setReg)(int,unsigned),unsigned(*getReg)(int),void(*runFrame)(void),
 unsigned globals,unsigned commands)
{
 unsigned char expected[0xf600];uint32_t random=0x729fa013;
 const WillowVisibilityBus bus={nvpWord,nvpByte,nvpPutWord,nvpPutByte};
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++){
    random^=random<<13;random^=random>>17;random^=random<<5;
    nvpSource[g->source+n]=ram[(globals+g->offset+n)^1]=random;
   }
  }
  /* Exercise reset through the actual native storage before each fixture,
   * then randomize publication inputs again to retain arbitrary-state scope. */
  memcpy(expected,ram,sizeof expected);
  WillowNativeBootGlobals reset;willowNativeBootGlobalsReset(&reset);
  for(unsigned n=0;n<sizeof reset.bytes;n++)expected[(globals+n)^1]=reset.bytes[n];
  ram[0xff80^1]=0x60;ram[0xff81^1]=0xfe;
  const unsigned char resetArgs[]={0,255,255,128};
  for(unsigned n=0;n<sizeof resetArgs;n++)ram[(0xff00+n)^1]=resetArgs[n];
  setReg(17,0x2700);setReg(15,0xffff00);setReg(16,fullSymbol(symbols,"sourceBootGlobalsResetProbe"));
  for(unsigned slice=0;slice<8&&getReg(16)!=0xffff80;slice++)runFrame();
  if(getReg(16)!=0xffff80||memcmp(expected,ram,sizeof expected))return 1;
  for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
   const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
   for(unsigned n=0;n<g->count;n++)ram[(globals+g->offset+n)^1]=nvpSource[g->source+n];
  }
  for(unsigned n=0;n<256;n++)nvpHardware[n]=ram[(commands+n)^1]=(sample*31+n*73)^0xa5;
  /* Repeated calls retain the previous staging and input history. */
  for(unsigned step=0;step<4;step++){
   memcpy(expected,ram,sizeof expected);
   willowArcadeVideoPublish(&bus,nvpInput,nvpOutByte,nvpOutWord);
   for(unsigned r=0;r<sizeof willowNativeBootGlobalRanges/sizeof willowNativeBootGlobalRanges[0];r++){
    const WillowNativeBootGlobalRange*g=willowNativeBootGlobalRanges+r;
    for(unsigned n=0;n<g->count;n++)expected[(globals+g->offset+n)^1]=nvpSource[g->source+n];
   }
   for(unsigned n=0;n<256;n++)expected[(commands+n)^1]=nvpHardware[n];
   ram[0xff80^1]=0x60;ram[0xff81^1]=0xfe;
   const unsigned char args[]={0,255,255,128};
   for(unsigned n=0;n<sizeof args;n++)ram[(0xff00+n)^1]=args[n];
   setReg(17,0x2700);setReg(15,0xffff00);setReg(16,fullSymbol(symbols,"sourceVideoPublishProbe"));
   for(unsigned slice=0;slice<8&&getReg(16)!=0xffff80;slice++)runFrame();
   if(getReg(16)!=0xffff80||memcmp(expected,ram,sizeof expected)){
    for(unsigned n=0;n<sizeof expected;n++)if(expected[n]!=ram[n]){
     fprintf(stderr,"Native publication fixture %u step %u RAM %04x got %02x want %02x\n",sample,step,n^1,ram[n],expected[n]);break;}
    return 1;
   }
  }
 }
 puts("Native retained reset: 256 compiled resets preserve unrelated RAM and match all 179 owned source bytes");
 puts("Native video publication: 1024 compiled calls through actual source bus; hardware output, input history, staged coordinates and unrelated non-stack RAM match");
 return 0;
}
