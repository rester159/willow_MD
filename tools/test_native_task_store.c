#include "../src/willow_native_task_store.h"
#include "../src/willow_native_task_segments.h"
#include "../src/willow_arcade_event_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WILLOW_TEST_SEGMENTED
static struct {
 uint8_t guard0[8],records[512],guard1[8],queue0[128],guard2[8],queue1[128],guard3[8];
 WillowNativeTaskControl control;uint8_t guard4[8];
} storage;
static WillowNativeTaskSegments segments={storage.records,&storage.control,{storage.queue0,storage.queue1}};
#define RESOLVE(a) willowNativeTaskSegmentResolve(&segments,a)
#define READ_WORD(a,v) willowNativeTaskSegmentReadWord(&segments,a,v)
#define WRITE_WORD(a,v) willowNativeTaskSegmentWriteWord(&segments,a,v)
#else
static WillowNativeTaskStore storage;
#define RESOLVE(a) willowNativeTaskResolve(&storage,a)
#define READ_WORD(a,v) willowNativeTaskReadWord(&storage,a,v)
#define WRITE_WORD(a,v) willowNativeTaskWriteWord(&storage,a,v)
#endif
static uint8_t oracle[65536],rom[0x100000];
static uint16_t rw(uint16_t a){uint16_t v;if(!READ_WORD(a,&v))abort();return v;}
static uint8_t rb(uint16_t a){uint8_t*p=RESOLVE(a);if(!p)abort();return *p;}
static void ww(uint16_t a,uint16_t v){if(!WRITE_WORD(a,v))abort();}
static void wb(uint16_t a,uint8_t v){uint8_t*p=RESOLVE(a);if(!p)abort();*p=v;}
static uint16_t ow(uint16_t a){return willowMotionWord(oracle+a);}
static uint8_t ob(uint16_t a){return oracle[a];}
static void opw(uint16_t a,uint16_t v){willowMotionPutWord(oracle+a,v);}
static void opb(uint16_t a,uint8_t v){oracle[a]=v;}
static uint16_t source(uint32_t a){return willowMotionWord(rom+a);}
static int mapped(unsigned a){return a<0x210||(a>=0xa10&&a<0xa30)||(a>=0x8072&&a<0x8172)||(a>=0x801a&&a<0x801e);}
static void match(void){for(unsigned a=0;a<65536;a++){uint8_t*p=RESOLVE(a);if(!!p!=mapped(a)||(p&&*p!=oracle[a]))abort();}}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
#ifdef WILLOW_TEST_SEGMENTED
 if(sizeof(WillowNativeTaskControl)!=52)return 1;
#else
 if(sizeof storage!=820)return 1;
#endif
 const WillowVisibilityBus compact={rw,rb,ww,wb},full={ow,ob,opw,opb};
 for(unsigned a=0;a<65536;a++){
  memset(&storage,0x5a,sizeof storage);__typeof__(storage) before=storage;
  uint16_t value=0x1234;
  int valid=mapped(a)&&mapped((uint16_t)(a+1));
  if(READ_WORD(a,&value)!=valid)return 1;
  if(!valid&&value!=0x1234)return 1;
  if(WRITE_WORD(a,0xbeef)!=valid)return 1;
  if(!valid&&memcmp(&before,&storage,sizeof storage))return 1;
 }
 for(unsigned sample=0;sample<64;sample++){
  for(unsigned a=0;a<65536;a++){oracle[a]=a*7+sample*31;uint8_t*p=RESOLVE(a);if(p)*p=oracle[a];}
  willowArcadeTaskInitStacks(&compact,source);willowArcadeTaskInitStacks(&full,source);
  willowArcadeTaskInitFree(&compact,source);willowArcadeTaskInitFree(&full,source);match();
  ww(0x801a,sample*4);opw(0x801a,sample*4);
  for(unsigned n=0;n<80;n++){
   willowArcadeEventCommand(&compact,n*977,n*317);willowArcadeEventCommand(&full,n*977,n*317);match();
  }
  for(unsigned n=0;n<9;n++){
   if(willowArcadeTaskAllocate(&compact,0x582c,n,n*977)!=willowArcadeTaskAllocate(&full,0x582c,n,n*977))return 1;match();
  }
  for(unsigned n=0;n<8;n++){
   ww(0x0204,0xff);opw(0x0204,0xff);ww(0x0206,0x100+n*32);opw(0x0206,0x100+n*32);
   willowArcadeTaskFinish(&compact,1);willowArcadeTaskFinish(&full,1);match();
  }
 }
#ifdef WILLOW_TEST_SEGMENTED
 const uint8_t*guards[]={storage.guard0,storage.guard1,storage.guard2,storage.guard3,storage.guard4};
 for(unsigned n=0;n<5;n++)for(unsigned j=0;j<8;j++)if(guards[n][j]!=0x5a)return 1;
 WillowNativeTaskSegments missing=segments;missing.queue[1]=0;uint8_t last=storage.queue0[127];uint16_t value=0x1234;
 if(willowNativeTaskSegmentReadWord(&missing,0x80f1,&value)||value!=0x1234||willowNativeTaskSegmentWriteWord(&missing,0x80f1,0xabcd)||storage.queue0[127]!=last)return 1;
 puts("Segmented task store: 52 control bytes, 512 record bytes and two 128-byte queue halves; guards and missing-half atomicity pass");
#endif
 puts("Compact task store: 820 bytes; exhaustive address/word boundaries, 64 initialization/allocation/release sequences and 5120 queue writes match source-addressed storage");
 return 0;
}
