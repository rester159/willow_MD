/* Independent iterator proof shared by host sanitizers and actual68000.
 * The oracle computes each source record and its address independently. */
#include "willow_native_object_latch_next.h"
#if !defined(__m68k__)
#include <stdio.h>
static uint32_t hostRam[16384];
#endif
static unsigned cases,reads,rejections;
static uint16_t failure,casePattern,caseLimit,caseFirst;
static volatile uint8_t*testRam(void)
{
#if defined(__m68k__)
 return (volatile uint8_t*)0xff0000;
#else
 return (volatile uint8_t*)hostRam;
#endif
}
static uint16_t recordOffset(uint16_t n)
{return n==256?0x3620:(uint16_t)(0x2660+(n/8)*128+(n%8)*8);}
static void expectedRecord(uint16_t pattern,uint16_t record,uint16_t words[4])
{
 uint16_t live=0;
 switch(pattern){
 case 0:break;
 case 1:live=1;break;
 case 2:live=record==255;break;
 case 3:live=record==7||record==8||record==15||record==16||record==247||record==248;break;
 case 4:live=(record&7)==7;break;
 case 5:live=!(record&7);break;
 case 6:live=(((record*73u)^0x51a5u)&7)<2;break;
 case 7:live=record==0||record==7||record==127||record==255;break;
 }
 for(unsigned n=0;n<4;n++)words[n]=live?(uint16_t)(0x1000+record*17+n*0x211):0;
 if(pattern==7&&live){words[0]=words[1]=words[2]=0;words[3]=0xff00;}
}
static uint8_t isZero(const uint16_t*words)
{return !(words[0]|words[1]|words[2]|words[3]);}
static uint16_t spanExpected(uint16_t pattern,uint16_t offset)
{
 if(offset<0x2660||offset>=0x3620||((offset-0x2660)&127)>=64)return 0xa55a;
 uint16_t delta=(uint16_t)(offset-0x2660),record=(uint16_t)((delta/128)*8+(delta%128)/8),words[4];
 expectedRecord(pattern,record,words);return words[(delta&7)/2];
}
static uint8_t invalidCases(WillowObjectLatchSequential*reader,uint16_t first,uint16_t limit)
{
 for(unsigned variant=0;variant<7;variant++){
  uint16_t storage[8],resolved=0xd00d;
  for(unsigned n=0;n<8;n++)storage[n]=(uint16_t)(0xa700+n);
  WillowObjectLatchSequential saved=*reader;
  uint16_t badFirst=first,badLimit=limit;uint8_t invisible=1;
  void*context=reader;uint16_t*out=storage+2,*index=&resolved;
  if(variant==0)context=0;
  if(variant==1)out=0;
  if(variant==2)index=0;
  if(variant==3)badFirst=(uint16_t)(first==256?0:first+1);
  if(variant==4)badLimit=257;
  if(variant==5){badFirst=1;badLimit=0;}
  if(variant==6)invisible=2;
  if(willowNativeObjectLatchNext(context,badFirst,badLimit,invisible,out,index))return 0;
  if(reader->next!=saved.next||reader->base!=saved.base||reader->record!=saved.record||resolved!=0xd00d)return 0;
  for(unsigned n=0;n<8;n++)if(storage[n]!=(uint16_t)(0xa700+n))return 0;
  rejections++;
 }
 return 1;
}
uint16_t nativeObjectPresentation(void)
{
 volatile uint8_t*ram=testRam();cases=reads=rejections=0;failure=0;
 for(uint16_t pattern=0;pattern<8;pattern++){
  casePattern=pattern;
  for(uint16_t at=0x2600;at<0x3680;at+=2)
   *(volatile WillowObjectLatchWord*)(ram+at)=spanExpected(pattern,at);
  for(uint16_t limit=0;limit<=256;limit++)for(uint8_t invisible=0;invisible<2;invisible++){
   caseLimit=limit;WillowObjectLatchSequential reader;
   if(!willowNativeObjectLatchSequentialInit(&reader,ram)){failure=1;return 0;}
   uint16_t first=0;
   if(!invalidCases(&reader,first,limit)){failure=2;return 0;}
   for(;;){
    caseFirst=first;
    uint16_t resolved=0xd00d,expected=first,words[4],storage[8];
    unsigned start=2+(limit&1);
    for(unsigned n=0;n<8;n++)storage[n]=(uint16_t)(0xa700+n);
    while(expected<limit){expectedRecord(pattern,expected,words);if(!invisible||!isZero(words))break;expected++;}
    uint8_t status=willowNativeObjectLatchNext(&reader,first,limit,invisible,storage+start,&resolved);
    if(status!=(expected<limit?1:2)||resolved!=expected){failure=3;return 0;}
    uint16_t next=(uint16_t)(expected<limit?expected+1:limit);
    if(reader.record!=next||reader.base!=ram||
       reader.next!=(const volatile WillowObjectLatchWord*)(ram+recordOffset(next))){failure=4;return 0;}
    for(unsigned n=0;n<8;n++){
     uint16_t value=(uint16_t)(0xa700+n);
     if(status==1&&n>=start&&n<start+4)value=words[n-start];
     if(storage[n]!=value){failure=5;return 0;}
    }
    reads++;
    if(status==2)break;
    first=next;
   }
   if(!invalidCases(&reader,limit,limit)){failure=6;return 0;}
   cases++;
  }
  for(uint16_t at=0x2600;at<0x3680;at+=2)
   if(*(const volatile WillowObjectLatchWord*)(ram+at)!=spanExpected(pattern,at)){failure=7;return 0;}
 }
 /* A failed initializer must not become a valid zero-length scanner. */
 WillowObjectLatchSequential bad;uint16_t out[4]={1,2,3,4},resolved=0xd00d;
 if(willowNativeObjectLatchSequentialInit(&bad,0)||
    willowNativeObjectLatchNext(&bad,256,256,1,out,&resolved)||resolved!=0xd00d){failure=8;return 0;}
#if defined(__m68k__)
 *(volatile uint32_t*)0xff0008=cases;
 *(volatile uint32_t*)0xff000c=reads;
 *(volatile uint32_t*)0xff0010=rejections;
#endif
 return 1;
}
#if !defined(__m68k__)
int main(void)
{
 if(!nativeObjectPresentation()){
  fprintf(stderr,"Iterator failure%u pattern%u limit%u first%u\n",failure,casePattern,caseLimit,caseFirst);return 1;
 }
 printf("{\"cases\":%u,\"reads\":%u,\"rejections\":%u}\n",cases,reads,rejections);return 0;
}
#endif
