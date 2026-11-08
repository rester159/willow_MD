/* Compare compact raw ROM transfers with the source attract palette service.
 * The latter is independently checked against original 68000 instructions. */
#include "../src/willow_native_palette_mutable.h"
#include "../src/willow_arcade_attract_init.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t rom[0x100000],globals[65536];
static uint16_t colors[3072];
static WillowNativePaletteOwner owner;
static WillowMutablePaletteGroup slots[2];
static unsigned copies;
static uint16_t word(uint32_t at){return willowMotionWord(rom+at);}
static uint16_t rw(uint16_t at){return willowMotionWord(globals+at);}
static uint8_t rb(uint16_t at){return globals[at];}
static void ww(uint16_t at,uint16_t value){willowMotionPutWord(globals+at,value);}
static void wb(uint16_t at,uint8_t value){globals[at]=value;}
static void writeColor(uint32_t at,uint16_t value){colors[(at-0x91c000)/2]=value;}
static void copy(uint32_t from,uint32_t to,uint16_t count){
 if(!willowMutablePaletteReplaceROM(&owner,slots,2,from,to,count))abort();
 copies++;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Attract palette failure line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 const WillowVisibilityBus bus={rw,rb,ww,wb};
 for(unsigned trial=0;trial<384;trial++){
  for(unsigned g=0;g<192;g++){
   CHECK(willowNativePaletteSourceBind(&owner.sources,0x91c000+g*32,0xe5080+(g%16)*32));
   willowNativePaletteOwnerOffset(&owner,g,(trial+g)&15);
  }
  slots[0].group=slots[1].group=65535;
  for(unsigned n=0;n<2;n++){
   uint16_t data[16];for(unsigned c=0;c<16;c++)data[c]=(trial*977+n*311+c*257)&65535;
   CHECK(willowMutablePaletteInstall(&owner,slots,2,0x91c000+((trial/2+n*97)%192)*32,data));
  }
  for(unsigned c=0;c<3072;c++)CHECK(willowMutablePaletteRead(&owner,slots,2,0x91c000+c*2,word,colors+c));
  WillowMutablePaletteGroup before[2];memcpy(before,slots,sizeof slots);
  globals[0x8343]=trial&1;
  CHECK(willowArcadeAttractPalette(&bus,word,writeColor));
  CHECK(willowArcadeAttractPaletteWithCopy(&bus,word,writeColor,copy));
  for(unsigned c=0;c<3072;c++){
   uint16_t actual;CHECK(willowMutablePaletteRead(&owner,slots,2,0x91c000+c*2,word,&actual));CHECK(actual==colors[c]);
  }
  for(unsigned n=0;n<2;n++){
   unsigned replaced=before[n].group>=16&&before[n].group<128;
   CHECK(slots[n].group==(replaced?65535:before[n].group));
   CHECK(!memcmp(slots[n].words,before[n].words,sizeof slots[n].words));
  }
 }
 const uint32_t invalid[][3]={{0xdfffe,0x91c000,16},{0xffff0,0x91c000,16},
  {0xe5081,0x91c000,16},{0xe5080,0x91bfe0,16},{0xe5080,0x91d800,16},
  {0xe5080,0x91c002,16},{0xe5080,0x91d7e0,32},{0xe5080,0x91c000,0},
  {0xe5080,0x91c000,17},{0xffffffff,0x91c000,16}};
 WillowNativePaletteOwner saved=owner;WillowMutablePaletteGroup savedSlots[2];memcpy(savedSlots,slots,sizeof slots);
 for(unsigned n=0;n<sizeof invalid/sizeof invalid[0];n++){
  CHECK(!willowMutablePaletteReplaceROM(&owner,slots,2,invalid[n][0],invalid[n][1],invalid[n][2]));
  CHECK(!memcmp(&saved,&owner,sizeof owner)&&!memcmp(savedSlots,slots,sizeof slots));
 }
 CHECK(!willowMutablePaletteReplaceROM(&owner,NULL,2,0xe5080,0x91c000,16));
 printf("Attract ROM ownership: 384 cases, %u source transfers, 1179648 exact CPS word comparisons; both mutable slots preserved or released by overwritten range; 11 invalid calls reject\n",copies);
 return 0;
}
