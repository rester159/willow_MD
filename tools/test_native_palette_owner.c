#include "../src/willow_native_palette_owner.h"
#include "../src/willow_arcade_palette_animation.h"
#include <stdio.h>
#include <string.h>
static uint8_t rom[0x100000];
static uint16_t colors[3072];
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t readColor(uint32_t a){return colors[(a-0x91c000)/2];}
static void writeColor(uint32_t a,uint16_t v){colors[(a-0x91c000)/2]=v;}
static uint32_t rng=0x384c92;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 WillowNativePaletteOwner owner;WillowPaletteAnimation g={.word=word,.read=readColor,.write=writeColor};
 unsigned accepted=0,rejected=0;
 if(sizeof owner!=480)return 1;
 for(unsigned trial=0;trial<8192;trial++){
  memset(&owner,0xff,sizeof owner.sources);memset(&owner.view,0,sizeof owner.view);
  unsigned group=next()%192;uint32_t destination=0x91c000+group*32;
  uint32_t oldSource=trial<4096?0xe5080+(next()%16)*32:0xe0000+(next()%4096)*32;
  uint32_t source=trial<4096?0xe5480+(next()%112)*32:0xe0000+(next()%4096)*32;
  uint8_t offset=next()&15;
  if(!willowNativePaletteSourceBind(&owner.sources,destination,oldSource))return 1;
  willowNativePaletteOwnerOffset(&owner,group,offset);
  for(unsigned n=0;n<16;n++)colors[group*16+n]=(uint16_t)(word(oldSource+n*2)+(offset<<12));
  WillowNativePaletteOwner before=owner;
  willowPaletteCopy(source,destination,&g);
  uint16_t delta=(uint16_t)(colors[group*16]-word(source));int representable=1;
  for(unsigned n=0;n<16;n++)if((uint16_t)(colors[group*16+n]-word(source+n*2))!=delta)representable=0;
  int result=willowNativePaletteOwnerCopy(&owner,source,destination,word);
  if(result!=representable || (trial<4096 && !result))return 1;
  if(!result){if(memcmp(&owner,&before,sizeof owner))return 1;rejected++;continue;}
  accepted++;
  for(unsigned n=0;n<16;n++){
   uint16_t value;if(!willowNativePaletteOwnerRead(&owner,destination+n*2,word,&value)||value!=colors[group*16+n])return 1;
  }
  for(unsigned other=0;other<192;other++)if(other!=group && (owner.sources.group[other]!=before.sources.group[other] || willowNativePaletteOffset(&owner.view,other)!=willowNativePaletteOffset(&before.view,other)))return 1;
 }
 for(unsigned first=0;first<7;first++)for(unsigned second=0;second<7;second++){
  for(unsigned group=0;group<192;group++){
   if(!willowNativePaletteSourceBind(&owner.sources,0x91c000+group*32,0xe5080+(group%16)*32))return 1;
   willowNativePaletteOwnerOffset(&owner,group,group&15);
   for(unsigned n=0;n<16;n++)if(!willowNativePaletteOwnerRead(&owner,0x91c000+group*32+n*2,word,colors+group*16+n))return 1;
  }
  const unsigned stages[]={first,second};uint16_t masks[4];
  for(unsigned step=0;step<2;step++){
   if(!willowNativePaletteOwnerStage(&owner,stages[step],word)||!willowPaletteStageInit(stages[step],masks,word,writeColor))return 1;
   for(unsigned n=0;n<3072;n++){uint16_t value;if(!willowNativePaletteOwnerRead(&owner,0x91c000+n*2,word,&value)||value!=colors[n])return 1;}
  }
 }
 puts("ROM palette owner: all 49 ordered stage transitions preserve untouched colors and reset copied brightness, 98 full palette comparisons");
 memset(&owner,0xff,sizeof owner);WillowNativePaletteOwner before=owner;
 if(willowNativePaletteOwnerStage(&owner,7,word)||memcmp(&owner,&before,sizeof owner))return 1;
 if(willowNativePaletteOwnerCopy(&owner,0xe5080,0x91c000,word)||memcmp(&owner,&before,sizeof owner))return 1;
 if(willowNativePaletteOwnerCopy(&owner,0xffffe,0x91c000,word)||willowNativePaletteOwnerCopy(&owner,0xe5080,0x91c002,word))return 1;
 printf("ROM palette owner: 480 bytes; 8192 C92 copies (%u exact bindings, %u atomic rejections), all 4096 stage-palette cases supported\n",accepted,rejected);
 return accepted && rejected?0:1;
}
