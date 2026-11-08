#include "../src/willow_native_palette_sources.h"
#include <stdio.h>
#include <string.h>
static uint8_t rom[0x100000];
static uint16_t expected[3072];
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static void writeColor(uint32_t a,uint16_t v){expected[(a-0x91c000)/2]=v;}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 WillowNativePaletteSources sources;uint16_t masks[4];unsigned checks=0;
 if(sizeof sources!=384)return 1;
 for(unsigned first=0;first<7;first++)for(unsigned next=0;next<7;next++){
  for(unsigned group=0;group<192;group++){
   if(!willowNativePaletteSourceBind(&sources,0x91c000+group*32,0xe0000+group*32))return 1;
   for(unsigned color=0;color<16;color++)expected[group*16+color]=word(0xe0000+group*32+color*2);
  }
  const unsigned stages[]={first,next};
  for(unsigned n=0;n<2;n++){
   if(!willowNativePaletteStageSources(&sources,stages[n],word)||!willowPaletteStageInit(stages[n],masks,word,writeColor))return 1;
   for(unsigned color=0;color<3072;color++){
    uint16_t value;if(!willowNativePaletteSourceRead(&sources,0x91c000+color*2,word,&value)||value!=expected[color])return 1;
   }
   checks++;
  }
 }
 memset(&sources,0xff,sizeof sources);uint16_t value=0x1234;
 if(willowNativePaletteSourceRead(&sources,0x91c000,word,&value)||value!=0x1234)return 1;
 for(unsigned tag=0xfff1;tag<0xffff;tag++){
  sources.group[0]=tag;value=0x1234;
  if(willowNativePaletteSourceRead(&sources,0x91c000,word,&value)||value!=0x1234)return 1;
 }
 sources.group[0]=0xffff;
 WillowNativePaletteSources before=sources;
 if(willowNativePaletteStageSources(&sources,7,word)||memcmp(&before,&sources,sizeof sources))return 1;
 if(willowNativePaletteSourceBind(&sources,0x91c002,0xe0000)||willowNativePaletteSourceBind(&sources,0x91c000,0xffffe))return 1;
 puts("ROM palette source map: 384 bytes; all 49 stage transitions, 98 full 3072-word comparisons including untouched groups pass");
 return checks==98?0:1;
}
