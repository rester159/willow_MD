/* Check generated lookup coverage against original ROM bytes and observed
 * access footprints, including every hole, page boundary and odd address. */
#include "../native/intro_probe/src/demo_update_data.h"
#ifdef WILLOW_TEST_EXTENDED_DEMO_UPDATE
#include "../native/intro_probe/src/demo_sequence_extended_data.h"
#else
#include "../native/intro_probe/src/demo_sequence_data.h"
#endif
#include <stdint.h>
#include <stdio.h>
static uint8_t program[0x100000],access[0x100000];
static int load(const char*path,uint8_t*out)
{
 FILE*f=fopen(path,"rb");if(!f)return 0;
 int ok=fread(out,1,0x100000,f)==0x100000&&fgetc(f)==EOF;fclose(f);return ok;
}
int main(void)
{
#ifdef WILLOW_TEST_EXTENDED_DEMO_UPDATE
 nativeDemoSequenceDataInit();
#endif
 if(!load("build/reference_maincpu.bin",program))return 2;
 unsigned char(*readers[2])(unsigned long,unsigned short*)={nativeDemoUpdateWordRead,nativeDemoSequenceWordRead};
 const char*paths[]={"build/demo-update-rom-access.bin",
#ifdef WILLOW_TEST_EXTENDED_DEMO_UPDATE
 "build/demo-extended-investigation/sequence-rom-access.bin"
#else
 "build/demo-sequence-rom-access.bin"
#endif
};
 for(unsigned pass=0;pass<2;pass++){
  if(!load(paths[pass],access))return 2;
  unsigned words=0;
  for(unsigned at=0;at<sizeof program;at++){
   unsigned short value=0xa55a;
   int wanted=!(at&1)&&(access[at]||access[at+1]);
   int found=readers[pass](at,&value);
   unsigned expected=wanted?((unsigned)program[at]<<8)|program[at+1]:0xa55a;
   if(found!=wanted||value!=expected||readers[pass](at,NULL)){
    fprintf(stderr,"ROM lookup %u address %x got %d/%04x expected %d/%04x\n",pass,at,found,value,wanted,expected);return 1;
   }
   words+=wanted;
  }
  const unsigned long outside[]={0x100000UL,0x100002UL,0xffffffUL,0xffffffffUL};
  for(unsigned n=0;n<4;n++){
   unsigned short value=0xa55a;
   if(readers[pass](outside[n],&value)||value!=0xa55a)return 1;
  }
  printf("ROM lookup %u: all 1048576 byte addresses and out-of-range/null reads checked; %u exact original words\n",pass,words);
 }
 return 0;
}
