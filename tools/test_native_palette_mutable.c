#include "../src/willow_native_palette_fade.h"
#include "../src/willow_arcade_palette_animation.h"
#include <stdio.h>
#include <string.h>
static uint8_t rom[0x100000];
static uint16_t expected[3072];
static WillowNativePaletteOwner owner;
static WillowMutablePaletteGroup slots[192];
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint16_t readColor(uint32_t a){return expected[(a-0x91c000)/2];}
static void writeColor(uint32_t a,uint16_t v){expected[(a-0x91c000)/2]=v;}
static uint32_t rng=0xc921234;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Palette mutable failure line %d\n",__LINE__);return 1;}}while(0)
static int same(void){
 for(unsigned n=0;n<3072;n++){
  uint16_t value;if(!willowMutablePaletteRead(&owner,slots,192,0x91c000+n*2,word,&value)||value!=expected[n])return 0;
 }
 return 1;
}
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 CHECK(sizeof(WillowMutablePaletteGroup)==34);
 for(unsigned group=0;group<192;group++){
  slots[group].group=0xffff;
  uint32_t source=0xe0000+(next()%4096)*32;
  CHECK(willowNativePaletteSourceBind(&owner.sources,0x91c000+group*32,source));
  willowNativePaletteOwnerOffset(&owner,group,group&15);
  for(unsigned n=0;n<16;n++)expected[group*16+n]=(uint16_t)(word(source+n*2)+((group&15)<<12));
 }
 WillowPaletteAnimation g={.word=word,.read=readColor,.write=writeColor};
 unsigned counts[4]={0},peak=0;
 for(unsigned step=0;step<8192;step++){
  unsigned group=(next()>>16)%192,op=(next()>>24)%4;uint32_t destination=0x91c000+group*32;
  counts[op]++;
  if(op==0){
   unsigned color=(next()>>16)&15;uint16_t value=next()>>8;
   CHECK(willowMutablePaletteWrite(&owner,slots,192,destination+color*2,value,word));
   expected[group*16+color]=value;
  }else if(op==1){
   uint16_t delta=(next()&1)?0x1000:0xf000;
   CHECK(willowNativePaletteAdjust(&owner.view,destination,delta));
   for(unsigned n=0;n<16;n++)expected[group*16+n]+=delta;
  }else if(op==2){
   uint32_t source=0xe0000+((next()>>12)%4096)*32;
   willowPaletteCopy(source,destination,&g);
   CHECK(willowMutablePaletteCopy(&owner,slots,192,source,destination,word));
  }else{
   unsigned stage=(next()>>16)%7;uint16_t masks[4];
   CHECK(willowPaletteStageInit(stage,masks,word,writeColor));
   CHECK(willowMutablePaletteStage(&owner,slots,192,stage,word));
  }
  CHECK(same());unsigned used=0;
  for(unsigned n=0;n<192;n++)if(slots[n].group!=0xffff){
   CHECK(slots[n].group<192);used++;
   for(unsigned j=0;j<n;j++)CHECK(slots[j].group!=slots[n].group);
  }
  if(used>peak)peak=used;
 }
 /* A single slot: exact exhaustion, no-op write, reuse, and promotion. */
 memset(&owner,0xff,sizeof owner);memset(slots,0xff,sizeof slots);
 uint16_t values[16];for(unsigned n=0;n<16;n++)values[n]=0x4000+n;
 CHECK(willowMutablePaletteInstall(&owner,slots,1,0x91c000,values));
 CHECK(willowNativePaletteSourceBind(&owner.sources,0x91c020,0xe5080));
 willowNativePaletteOwnerOffset(&owner,1,0);
 WillowNativePaletteOwner before=owner;WillowMutablePaletteGroup saved=slots[0];
 CHECK(!willowMutablePaletteWrite(&owner,slots,1,0x91c020,word(0xe5080)^1,word));
 CHECK(!memcmp(&before,&owner,sizeof owner)&&!memcmp(&saved,slots,sizeof saved));
 CHECK(willowMutablePaletteWrite(&owner,slots,1,0x91c020,word(0xe5080),word));
 CHECK(!memcmp(&before,&owner,sizeof owner)&&!memcmp(&saved,slots,sizeof saved));
 CHECK(willowMutablePaletteCopy(&owner,slots,1,0xe5080,0x91c000,word));
 CHECK(slots[0].group==0xffff);
 CHECK(willowMutablePaletteWrite(&owner,slots,1,0x91c020,word(0xe5080)^1,word));
 CHECK(slots[0].group==1);
 before=owner;saved=slots[0];uint16_t untouched=0xabcd;
 CHECK(!willowMutablePaletteRead(&owner,slots,1,0x91c040,word,&untouched)&&untouched==0xabcd);
 CHECK(!willowMutablePaletteWrite(&owner,slots,1,0x91c040,123,word));
 CHECK(!willowMutablePaletteStage(&owner,slots,1,7,word));
 CHECK(!willowMutablePaletteCopy(&owner,slots,1,0xffffe,0x91c000,word));
 CHECK(!willowMutablePaletteWrite(&owner,slots,1,0x91c001,123,word));
 CHECK(!memcmp(&before,&owner,sizeof owner)&&!memcmp(&saved,slots,sizeof saved));
 /* Force a mixed-brightness source group and exhaust its copy fallback. */
 for(unsigned n=0;n<16;n++)willowMotionPutWord(rom+0xff000+n*2,(n<<12)|0x123);
 CHECK(willowNativePaletteSourceBind(&owner.sources,0x91c040,0xff000));
 willowNativePaletteOwnerOffset(&owner,2,0);before=owner;saved=slots[0];
 CHECK(!willowMutablePaletteCopy(&owner,slots,1,0xe5080,0x91c040,word));
 CHECK(!memcmp(&before,&owner,sizeof owner)&&!memcmp(&saved,slots,sizeof saved));
 CHECK(willowMutablePaletteCopy(&owner,slots,1,0xe5080,0x91c020,word));
 CHECK(slots[0].group==0xffff);
 CHECK(willowMutablePaletteCopy(&owner,slots,1,0xe5080,0x91c040,word));
 CHECK(slots[0].group==2);
 for(unsigned n=0;n<16;n++){
  uint16_t value;CHECK(willowMutablePaletteRead(&owner,slots,1,0x91c040+n*2,word,&value));
  CHECK(value==(uint16_t)(word(0xe5080+n*2)|(n<<12)));
 }
 CHECK(willowMutablePaletteStage(&owner,slots,1,0,word)&&slots[0].group==0xffff);
 /* Whole-fade validation is atomic even at the final participating group. */
 memset(&owner,0,sizeof owner);memset(slots,0xff,sizeof slots);
 for(unsigned n=0;n<192;n++)CHECK(willowNativePaletteSourceBind(&owner.sources,0x91c000+n*32,0xe5080));
 owner.sources.group[167]=0xffff;before=owner;saved=slots[0];
 WillowPaletteFade fade={.masks={1,2,3,4,5,6},.sum=0xdeadbeef},oldFade=fade;
 CHECK(!willowNativePaletteFadeAll(&owner,slots,1,&fade,0x1000,word));
 CHECK(!memcmp(&before,&owner,sizeof owner)&&!memcmp(&saved,slots,sizeof saved)&&!memcmp(&fade,&oldFade,sizeof fade));
 CHECK(willowNativePaletteSourceBind(&owner.sources,0x91c000+167*32,0xe5080));
 owner.sources.group[191]=0xffff;
 CHECK(willowNativePaletteFadeAll(&owner,slots,1,&fade,0x1000,word));
 CHECK(owner.sources.group[191]==0xffff&&willowNativePaletteOffset(&owner.view,191)==0);
 printf("Mutable palette owner: 8192 mixed operations (%u writes, %u fades, %u copies, %u stage installs), 25165824 exact word comparisons; peak %u exceptional groups in this synthetic sequence\n",counts[0],counts[1],counts[2],counts[3],peak);
 puts("Explicit capacity exhaustion and unowned access are atomic; no-op writes, ROM promotion, slot reuse and stage retirement pass; 34 bytes per exceptional group");
 return 0;
}
