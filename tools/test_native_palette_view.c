#include "../src/willow_native_palette_view.h"
#include "../src/willow_arcade_palette_fade.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static WillowNativePaletteView view;
static uint16_t base[3072],expected[3072];
static uint16_t readBase(uint32_t a){return base[(a-0x91c000)/2];}
static void writeBase(uint32_t a,uint16_t v){base[(a-0x91c000)/2]=v;}
static uint16_t readView(uint32_t a){return willowNativePaletteRead(&view,a,readBase);}
static void writeView(uint32_t a,uint16_t v){willowNativePaletteWrite(&view,a,v,writeBase);}
static void adjust(uint32_t a,uint16_t d){if(!willowNativePaletteAdjust(&view,a,d))abort();}
static uint16_t readExpected(uint32_t a){return expected[(a-0x91c000)/2];}
static void writeExpected(uint32_t a,uint16_t v){expected[(a-0x91c000)/2]=v;}
int main(void){
 if(sizeof view!=96)return 1;
 unsigned checks=0;
 for(unsigned sample=0;sample<64;sample++){
  memset(&view,0,sizeof view);
  for(unsigned n=0;n<3072;n++)base[n]=expected[n]=sample*977+n*317;
  WillowPaletteFade lazy={0},full={0};
  lazy.read=readView;lazy.write=writeView;lazy.adjustGroup=adjust;
  full.read=readExpected;full.write=writeExpected;
  for(unsigned bank=0;bank<6;bank++)lazy.masks[bank]=full.masks[bank]=sample*0x1234567u+bank*977;
  for(unsigned step=0;step<96;step++){
   uint16_t delta=(step&16)?0x1000:0xf000;
   willowPaletteFadeAll(&lazy,delta);willowPaletteFadeAll(&full,delta);
   unsigned index=(sample*31+step*137)%3072;uint16_t value=sample*777+step*313;
   writeView(0x91c000+index*2,value);expected[index]=value;
   if(lazy.sum!=full.sum||memcmp(lazy.masks,full.masks,sizeof lazy.masks))return 1;
   for(unsigned n=0;n<3072;n++)if(readView(0x91c000+n*2)!=expected[n])return 1;
   checks++;
  }
 }
 for(unsigned group=0;group<192;group++)for(unsigned delta=0;delta<16;delta++){
  memset(&view,0,sizeof view);
  if(!willowNativePaletteAdjust(&view,0x91c000+group*32,delta<<12))return 1;
  for(unsigned n=0;n<192;n++)if(willowNativePaletteOffset(&view,n)!=(n==group?delta:0))return 1;
 }
 const uint32_t invalid[]={0,0x91bfe0,0x91c002,0x91d800,0xffffffff};
 WillowNativePaletteView saved=view;
 for(unsigned n=0;n<sizeof invalid/sizeof *invalid;n++)if(willowNativePaletteAdjust(&view,invalid[n],0x1000)||memcmp(&saved,&view,sizeof view))return 1;
 if(willowNativePaletteAdjust(&view,0x91c000,1)||memcmp(&saved,&view,sizeof view))return 1;
 printf("Lazy CPS palette: 96 bytes, %u mixed fade/write steps match all 3072 words and masks; all 192 packed groups and 16 offsets verified\n",checks);
 return 0;
}
