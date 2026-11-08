#include "../src/willow_native_task_runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t ram[65536],rom[0x100000];
static uint16_t rw(uint16_t a){return willowMotionWord(ram+a);}
static uint8_t rb(uint16_t a){return ram[a];}
static void ww(uint16_t a,uint16_t v){willowMotionPutWord(ram+a,v);}
static void wb(uint16_t a,uint8_t v){ram[a]=v;}
static uint16_t source(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t color(uint32_t a){return rw(0x2000+a-0x91c000);}
static void putColor(uint32_t a,uint16_t v){ww(0x2000+a-0x91c000,v);}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 unsigned scenarios=0,dispatches=0;
 for(unsigned sample=0;sample<64;sample++){
  WillowNativeTaskRuntime runtime={0};memset(ram,0,sizeof ram);
  willowArcadeTaskInitFree(&bus,source);
  for(unsigned n=0;n<128;n++)ww(0x8072+n*2,0xffff);
  ww(0x801a,sample*4);ww(0x801c,sample*4);
  willowArcadeTaskStart(&bus,0xa0,0x1ddc,0,0,1);
  ww(0x82c0,1);
  for(unsigned bank=0;bank<6;bank++){ww(0x82a8+bank*4,bank<4?65535:0);ww(0x82aa+bank*4,bank<4?65535:255);}
  for(unsigned n=0;n<3072;n++)ww(0x2000+n*2,(sample%16)*4096+(n&4095));
  willowArcadeEventCommandWaitBegin(&bus,0x1eaa,0);
  unsigned pass;
  for(pass=0;pass<80&&rb(0xf891);pass++){
   uint16_t cursor=0,remaining=15;
   for(;;){
    WillowNativeTaskRun run=willowNativeTaskRunOne(&runtime,&bus,cursor,remaining,source,color,putColor);
    if(run.unsupported)return 1;
    if(!run.dispatched)break;
    dispatches++;if(run.remaining==65535)break;cursor=run.cursor;remaining=run.remaining;
   }
   willowArcadeTaskWake(&bus);
  }
  if(pass==80||rw(0x020c)!=8||rw(0x82c0)!=0||rb(0xf890))return 1;
  /* A new task replaces its stale native continuation; unsupported source
   * dispatch must not mutate either the RAM or continuation labels. */
  uint8_t before[65536];WillowNativeTaskRuntime saved=runtime;
  willowArcadeTaskStart(&bus,0,0x7fda,0,0,1);memcpy(before,ram,sizeof ram);
  WillowNativeTaskRun run=willowNativeTaskRunOne(&runtime,&bus,0,15,source,color,putColor);
  if(!run.unsupported||run.dispatched||memcmp(before,ram,sizeof ram)||memcmp(&saved,&runtime,sizeof runtime))return 1;
  runtime.resume[0]=0xbeef;
  willowArcadeTaskStart(&bus,0,0x1ddc,0,0,1);
  run=willowNativeTaskRunOne(&runtime,&bus,0,15,source,color,putColor);
  if(run.unsupported||!run.dispatched||runtime.resume[0]!=0x1e18)return 1;
  runtime.resume[0]=0x5994;wb(0,4);wb(19,0);ww(0x82c0,0);wb(0xf891,1);
  run=willowNativeTaskRunOne(&runtime,&bus,0,15,source,color,putColor);
  if(run.unsupported||!run.dispatched||rw(0)!=0x0200||runtime.resume[0]!=0x59a4)return 1;
  for(unsigned tick=0;tick<256;tick++)willowArcadeTaskWake(&bus);
  if(rw(0)!=0x0200||!rb(0xf891))return 1;
  scenarios++;
 }
 unsigned missingPaletteChecks=0;
 for(unsigned stage=0;stage<6;stage++)for(unsigned missing=1;missing<4;missing++){
  const uint16_t pcs[]={0x509c,0x509c,0x50c6,0x5102,0x50e4,0x5126};
  memset(ram,0xa5,sizeof ram);wb(0x020e,0);
  WillowNativeTaskRuntime runtime={0};runtime.resume[0]=pcs[stage];
  wb(0,stage<2?12:4);ww(4,0);ww(6,pcs[stage]);ww(16,stage==1?2:0);
  uint8_t before[65536];memcpy(before,ram,sizeof ram);WillowNativeTaskRuntime saved=runtime;
  WillowNativeTaskRun run=willowNativeTaskRunOne(&runtime,&bus,0,0,source,
   missing&1?NULL:color,missing&2?NULL:putColor);
  if(!run.unsupported||run.dispatched||memcmp(before,ram,sizeof ram)||memcmp(&saved,&runtime,sizeof runtime))return 1;
  missingPaletteChecks++;
 }
 printf("Global palette dependency rejection: %u fresh/resumed cases preserve task, runtime and all RAM\n",missingPaletteChecks);
 printf("Native continuation runtime: %u reveal/fade scenarios, %u dispatches; original queue handlers, sleep/wake, slot release and unsupported-entry preservation pass\n",scenarios,dispatches);
 return 0;
}
