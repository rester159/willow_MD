#include "../src/willow_native_palette_split.h"
#include <stdio.h>
#include <string.h>
static uint8_t rom[0x100000];
static uint16_t word(uint32_t a){return willowMotionWord(rom+a);}
static uint32_t rng=0x34c92;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Split palette failure line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 FILE*f=fopen("build/reference_maincpu.bin","rb");if(!f||fread(rom,1,sizeof rom,f)!=sizeof rom)return 2;fclose(f);
 WillowNativePaletteOwner a={0},b;
 WillowMutablePaletteGroup contiguous[2];
 struct {uint8_t before[16];WillowMutablePaletteGroup value;uint8_t after[16];} fragments[2];
 memset(fragments,0x5a,sizeof fragments);memset(contiguous,0xff,sizeof contiguous);
 WillowSplitPaletteSlots split={{&fragments[1].value,&fragments[0].value}};
 for(unsigned n=0;n<2;n++)*split.slot[n]=contiguous[n];
 for(unsigned n=0;n<192;n++){
  CHECK(willowNativePaletteSourceBind(&a.sources,0x91c000+n*32,0xe0000+n*32));
  willowNativePaletteOwnerOffset(&a,n,n&15);
 }
 b=a;unsigned rejected=0,counts[6]={0};
 for(unsigned step=0;step<8192;step++){
  unsigned op=(next()>>24)%6,group=(next()>>16)%8;counts[op]++;
  uint32_t at=0x91c000+group*32,source=0xe0000+((next()>>12)%4096)*32;
  uint16_t value=next()>>8,values[16];for(unsigned n=0;n<16;n++)values[n]=next()>>12;
  WillowNativePaletteOwner old=b;WillowMutablePaletteGroup oldSlots[2]={*split.slot[0],*split.slot[1]};
  int one,two;
  if(op==0){at+=(value&15)*2;one=willowMutablePaletteWrite(&a,contiguous,2,at,value,word);two=willowSplitPaletteWrite(&b,&split,at,value,word);}
  else if(op==1){one=willowMutablePaletteCopy(&a,contiguous,2,source,at,word);two=willowSplitPaletteCopy(&b,&split,source,at,word);}
  else if(op==2){one=willowMutablePaletteInstall(&a,contiguous,2,at,values);two=willowSplitPaletteInstall(&b,&split,at,values);}
  else if(op==3){value%=8;one=willowMutablePaletteStage(&a,contiguous,2,value,word);two=willowSplitPaletteStage(&b,&split,value,word);}
  else if(op==4){
   WillowPaletteFade fa={.masks={1,0x12345678,0xffff,0xfedcba98,0xff,0},.sum=123},fb=fa;
   value=(value&1)?0x1000:0xf000;
   one=willowNativePaletteFadeAll(&a,contiguous,2,&fa,value,word);two=willowSplitPaletteFadeAll(&b,&split,&fb,value,word);
   CHECK(!memcmp(fa.masks,fb.masks,sizeof fa.masks)&&fa.sum==fb.sum);
  }else{one=willowNativePaletteAdjust(&a.view,at,0x1000);two=willowNativePaletteAdjust(&b.view,at,0x1000);}
  CHECK(one==two&& !memcmp(&a,&b,sizeof a));
  if(!two){rejected++;CHECK(!memcmp(&old,&b,sizeof old));for(unsigned n=0;n<2;n++)CHECK(!memcmp(oldSlots+n,split.slot[n],sizeof oldSlots[n]));}
  for(unsigned n=0;n<2;n++){
   CHECK(!memcmp(contiguous+n,split.slot[n],sizeof contiguous[n]));
   for(unsigned j=0;j<16;j++)CHECK(fragments[n].before[j]==0x5a&&fragments[n].after[j]==0x5a);
  }
  for(unsigned n=0;n<3072;n++){
   uint16_t x,y;CHECK(willowMutablePaletteRead(&a,contiguous,2,0x91c000+n*2,word,&x));
   CHECK(willowSplitPaletteRead(&b,&split,0x91c000+n*2,word,&y)&&x==y);
  }
 }
 WillowNativePaletteOwner before=b;WillowSplitPaletteSlots invalid={{split.slot[0],0}};uint16_t value=0x1234;
 CHECK(!willowSplitPaletteRead(&b,&invalid,0x91c000,word,&value)&&value==0x1234);
 CHECK(!willowSplitPaletteWrite(&b,&invalid,0x91c000,0,word));
 invalid.slot[1]=invalid.slot[0];CHECK(!willowSplitPaletteStage(&b,&invalid,0,word));CHECK(!memcmp(&before,&b,sizeof b));
 printf("Split palette: 8192 mixed operations, 25165824 exact color comparisons, %u atomic rejections; reversed nonadjacent slots, guards, masks, sums and missing/aliased pointers pass\n",rejected);
 return rejected?0:1;
}
