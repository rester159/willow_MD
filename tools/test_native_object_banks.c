#include "../src/willow_native_object_banks.h"
#include "../src/willow_arcade_video_publish.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {uint16_t before;WillowNativeObjectBanks data;uint16_t after;} store;
static WillowNativeObjectBanks expected;
static WillowNativeObjectPort port;
static uint16_t softwareSelector;
static uint16_t sourceWord(uint16_t at){assert(at==0x8022);return softwareSelector;}
static void hardwareWord(uint16_t at,uint16_t value){assert(willowObjectPortWrite(&port,at,value));}
int main(void)
{
 assert(sizeof store.data==6144);
 store.before=0x1234;store.after=0xabcd;
 for(unsigned n=0;n<1024;n++){
  store.data.bank[0][n]=n*73;store.data.bank[1][n]=n*31+7;
  store.data.displayed[n]=n^0xa5a5;
 }
 unsigned writes=0,fills=0,latches=0;
 for(unsigned bank=0;bank<2;bank++)for(unsigned word=0;word<1024;word++){
  expected=store.data;expected.bank[bank][word]=word*97+bank;
  assert(willowObjectWrite(&store.data,0x900000+bank*0x8000+word*2,word*97+bank));
  assert(!memcmp(&expected,&store.data,sizeof expected));writes++;
 }
 for(unsigned selector=0;selector<65536;selector++){
  expected=store.data;
  unsigned base=((selector*256)&0x3ffff)&~2047;
  unsigned supported=base==0||base==0x8000;
  if(supported)memcpy(expected.displayed,expected.bank[base?1:0],2048);
  assert(willowObjectLatch(&store.data,selector)==supported);
  assert(!memcmp(&expected,&store.data,sizeof expected));latches++;
 }
 /* Clear both source banks; previously latched pixels remain backed by the
  * old copy until an explicit hardware latch event. */
 assert(willowObjectLatch(&store.data,0x9000));
 for(unsigned bank=0;bank<2;bank++){
  expected=store.data;memset(expected.bank[bank],0,2048);
  assert(willowObjectFill(&store.data,0x900000+bank*0x8000,0,0,512));
  assert(!memcmp(&expected,&store.data,sizeof expected));fills++;
 }
 for(unsigned bank=0;bank<2;bank++)for(unsigned start=0;start<1024;start++){
  unsigned count=(1024-start)/2;
  expected=store.data;
  for(unsigned n=0;n<count;n++){expected.bank[bank][start+n*2]=start;expected.bank[bank][start+n*2+1]=~start;}
  assert(willowObjectFill(&store.data,0x900000+bank*0x8000+start*2,start,~start,count));
  assert(!memcmp(&expected,&store.data,sizeof expected));fills++;
  assert(!willowObjectFill(&store.data,0x900000+bank*0x8000+start*2,0,0,count+1));
  assert(!memcmp(&expected,&store.data,sizeof expected));
 }
 for(unsigned at=0;at<0x10000;at++){
  expected=store.data;unsigned valid=!(at&1)&&(at<0x800||(at>=0x8000&&at<0x8800));
  uint16_t*pointer=valid?store.data.bank[at>=0x8000]+((at&0x7ff)/2):NULL;
  assert(willowObjectWord(&store.data,0x900000+at)==pointer);
  if(!valid){assert(!willowObjectWrite(&store.data,0x900000+at,0));assert(!willowObjectFill(&store.data,0x900000+at,0,0,1));}
  assert(!memcmp(&expected,&store.data,sizeof expected));
 }
 /* A fast aperture check must not truncate the high half of an address.
  * Cover every high word with both bank endpoints, alignment and holes. */
 static const unsigned low[]={0,1,0x7fe,0x7ff,0x800,0x7ffe,0x8000,0x8001,0x87fe,0x8800,0xffff};
 for(unsigned high=0;high<65536;high++)for(unsigned n=0;n<sizeof low/sizeof low[0];n++){
  uint32_t at=((uint32_t)high<<16)|low[n];
  uint16_t*pointer=NULL;
  if(!(at&1)&&at>=0x900000&&at<0x900800)pointer=store.data.bank[0]+((at-0x900000)/2);
  if(!(at&1)&&at>=0x908000&&at<0x908800)pointer=store.data.bank[1]+((at-0x908000)/2);
  assert(willowObjectWord(&store.data,at)==pointer);
 }
 puts("Object address mapping: 65536 low addresses and 720896 high-word/boundary combinations match exact legacy pointers");
 assert(!willowObjectWrite(NULL,0x900000,0));assert(!willowObjectFill(NULL,0x900000,0,0,512));assert(!willowObjectLatch(NULL,0x9000));
 const WillowVisibilityBus bus={sourceWord,0,0,0};
 for(unsigned sample=0;sample<256;sample++){
  for(unsigned bank=0;bank<2;bank++)for(unsigned n=0;n<1024;n++)store.data.bank[bank][n]=sample*73+n*31+bank;
  port.selector=sample&1?0x9080:0x9000;
  softwareSelector=port.selector^0x80;
  expected=store.data;memcpy(expected.displayed,expected.bank[sample&1],2048);
  /* VBlank copies the existing hardware selection, before the IRQ's write. */
  assert(willowObjectPortLatch(&store.data,&port));
  assert(!memcmp(&expected,&store.data,sizeof expected));
  willowArcadeObjectBasePublish(&bus,hardwareWord);
  assert(port.selector==softwareSelector&&!memcmp(&expected,&store.data,sizeof expected));
  softwareSelector^=0x80; /* Main-side staging alone changes no hardware. */
  unsigned next=(sample&1)^1;
  assert(willowObjectFill(&store.data,0x900000+next*0x8000,0,0,512));
  memset(expected.bank[next],0,2048);
  assert(!memcmp(&expected,&store.data,sizeof expected));
  memcpy(expected.displayed,expected.bank[next],2048);
  assert(willowObjectPortLatch(&store.data,&port));
  assert(!memcmp(&expected,&store.data,sizeof expected));
  unsigned saved=port.selector;
  assert(!willowObjectPortWrite(&port,0x102,0)&&port.selector==saved);
  assert(willowObjectPortWrite(&port,0x100,0x91c0));
  assert(!willowObjectPortLatch(&store.data,&port));
  assert(!memcmp(&expected,&store.data,sizeof expected));
 }
 assert(!willowObjectPortWrite(NULL,0x100,0));assert(!willowObjectPortLatch(&store.data,NULL));
 puts("Object publication: 256 VBlank/IRQ/staging/clear sequences keep software selection, hardware port and displayed bytes separate; unsupported port/latch operations preserve storage");
 assert(store.before==0x1234&&store.after==0xabcd);
 printf("Object banks: %u writes, %u fills, %u selector/latch checks; source clears preserve displayed copy, full address/span rejection and guards pass\n",writes,fills,latches);
}
