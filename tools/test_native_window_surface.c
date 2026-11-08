#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint16_t vram[32768];
static unsigned address,commands,writes;
static void control(uint32_t word)
{
 if((word&0xc000ffffUL)!=0x40000003UL)abort();
 address=((word>>16)&0x3fff)|((word&3)<<14);commands++;
}
static void data(uint16_t word){vram[address/2]=word;address+=2;writes++;}
#define WILLOW_WINDOW_SURFACE_CONTROL(value) control(value)
#define WILLOW_WINDOW_SURFACE_DATA(value) data(value)
#include "willow_native_window_surface.h"
static uint16_t colors[64],expected[1120];
static volatile uint32_t epoch=17;
static const WillowPresentationPlan plan={colors,0,0,7};
static WillowPresentationLease lease={&plan,&epoch,17};
static WillowPresentationFrame frame={&lease,17,7};
static WillowWindowSurface surface;
static WillowWindowSurfaceTarget target;
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"Window surface failure at %d\n",__LINE__);return 1;}}while(0)
static uint32_t randomState=0x294ba198;
static uint32_t next(void){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
static int compare(void)
{
 for(unsigned i=0;i<32768;i++){
  uint16_t value=0xa55a;
  if(i>=0xd000/2&&i<0xd000/2+28*64&&(i-0xd000/2)%64<40){
   unsigned cell=i-0xd000/2;value=expected[cell/64*40+cell%64];
  }
  if(vram[i]!=value)return 0;
 }
 return 1;
}
int main(void)
{
 REQUIRE(sizeof surface.words==2240);
 for(unsigned base=0;base<=0xf000;base+=0x1000){
  REQUIRE(willowWindowSurfaceTargetInit(&target,base));
  for(unsigned row=0;row<28;row++){
   uint32_t cmd=target.rows[row];
   REQUIRE((((cmd>>16)&0x3fff)|((cmd&3)<<14))==base+row*128);
  }
 }
 REQUIRE(willowWindowSurfaceTargetInit(&target,0xd000));
 WillowWindowSurfaceTarget savedTarget=target;
 REQUIRE(!willowWindowSurfaceTargetInit(&target,0xd002));
 REQUIRE(!memcmp(&target,&savedTarget,sizeof target));
 REQUIRE(!willowWindowSurfaceTargetInit(0,0xd000));
 for(unsigned i=0;i<32768;i++)vram[i]=0xa55a;
 REQUIRE(willowWindowSurfaceInit(&surface,&lease));
 REQUIRE(willowWindowSurfacePublish(&surface,&target,&frame));
 REQUIRE(commands==28&&writes==1120&&compare());
 commands=writes=0;
 for(unsigned n=0;n<1120;n++){
  uint16_t word=(uint16_t)(next()|1);expected[n]=word;
  REQUIRE(willowWindowSurfaceSet(&surface,n%40,n/40,word));
  REQUIRE(willowWindowSurfacePublish(&surface,&target,&frame));
  REQUIRE(commands==n+1&&writes==(n+1)*40&&compare());
 }
 commands=writes=0;
 for(unsigned n=0;n<2048;n++){
  uint32_t rows=0;
  for(unsigned j=0;j<1+(n&31);j++){
   unsigned cell=next()%1120;uint16_t word=(uint16_t)next();
   if(word!=expected[cell])rows|=1UL<<(cell/40);
   expected[cell]=word;
   REQUIRE(willowWindowSurfaceStage(&surface,(cell/40)*64+cell%40,word));
  }
  unsigned count=0;for(unsigned r=0;r<28;r++)count+=(rows>>r)&1;
  unsigned before=commands;
  REQUIRE(willowWindowSurfacePublish(&surface,&target,&frame));
  REQUIRE(commands-before==count&&compare());
  before=commands;
  REQUIRE(willowWindowSurfacePublish(&surface,&target,&frame));
  REQUIRE(before==commands);
 }
 unsigned before=commands;
 for(unsigned n=0;n<1120;n++)REQUIRE(willowWindowSurfaceSet(&surface,n%40,n/40,expected[n]));
 REQUIRE(!surface.dirty&&willowWindowSurfacePublish(&surface,&target,&frame)&&commands==before);
 WillowWindowSurface saved=surface;
 REQUIRE(!willowWindowSurfaceSet(&surface,40,0,123));
 REQUIRE(!willowWindowSurfaceSet(&surface,0,28,123));
 REQUIRE(!willowWindowSurfaceStage(&surface,40,123));
 REQUIRE(!willowWindowSurfaceStage(&surface,0xffff,123));
 REQUIRE(!memcmp(&surface,&saved,sizeof saved));
 surface.dirty=1;epoch++;
 REQUIRE(!willowWindowSurfaceSet(&surface,0,0,123));
 REQUIRE(!willowWindowSurfacePublish(&surface,&target,&frame));
 epoch--;frame.ready=3;
 REQUIRE(!willowWindowSurfacePublish(&surface,&target,&frame));
 frame.ready=7;surface.dirty=0x10000000UL;
 REQUIRE(!willowWindowSurfacePublish(&surface,&target,&frame));
 REQUIRE(commands==before);
 surface.dirty=0;
 REQUIRE(willowWindowSurfaceLoad(&surface,&lease,expected));
 REQUIRE(surface.dirty==WILLOW_WINDOW_SURFACE_ALL_ROWS);
 REQUIRE(willowWindowSurfacePublish(&surface,&target,&frame)&&compare());
 REQUIRE(!willowWindowSurfaceLoad(&surface,&lease,0));
 REQUIRE(!willowWindowSurfaceLoad(0,&lease,expected));
 printf("{\"single_cell_updates\":1120,\"random_update_batches\":2048,\"target_layouts\":16,\"unchanged_uploads\":0,\"native_word_bytes\":2240}\n");
 return 0;
}
