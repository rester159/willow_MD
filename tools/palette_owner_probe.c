#include "../src/willow_native_palette_split.h"
#define OWNER ((WillowNativePaletteOwner*)0xff2000UL)
#define SLOTS ((WillowMutablePaletteGroup*)0xff2200UL)
#define CONTROL ((volatile uint16_t*)0xff1000UL)
static uint16_t word(uint32_t a){return *(volatile uint16_t*)a;}
#ifdef WILLOW_PROBE_SPLIT_PALETTE
static const WillowSplitPaletteSlots splitSlots={{(WillowMutablePaletteGroup*)0xff2200UL,(WillowMutablePaletteGroup*)0xff2300UL}};
#define SLOT_COUNT 2
#define SLOT(n) (*splitSlots.slot[n])
#define WRITE(a,v) willowSplitPaletteWrite(OWNER,&splitSlots,a,v,word)
#define COPY(s,a) willowSplitPaletteCopy(OWNER,&splitSlots,s,a,word)
#define STAGE(s) willowSplitPaletteStage(OWNER,&splitSlots,s,word)
#define READ(a,v) willowSplitPaletteRead(OWNER,&splitSlots,a,word,v)
#else
#define SLOT_COUNT 8
#define SLOT(n) SLOTS[n]
#define WRITE(a,v) willowMutablePaletteWrite(OWNER,SLOTS,8,a,v,word)
#define COPY(s,a) willowMutablePaletteCopy(OWNER,SLOTS,8,s,a,word)
#define STAGE(s) willowMutablePaletteStage(OWNER,SLOTS,8,s,word)
#define READ(a,v) willowMutablePaletteRead(OWNER,SLOTS,8,a,word,v)
#endif
void paletteOwnerProbe(void){
 uint16_t command=CONTROL[0],result=0;
 uint32_t source=((uint32_t)CONTROL[2]<<16)|CONTROL[3];
 uint32_t address=((uint32_t)CONTROL[4]<<16)|CONTROL[5];
 if(!command){
  for(uint16_t n=0;n<SLOT_COUNT;n++)SLOT(n).group=0xffff;
  for(uint16_t n=0;n<192;n++){
   willowNativePaletteSourceBind(&OWNER->sources,0x91c000UL+n*32UL,0xe0000UL+n*32UL);
   willowNativePaletteOwnerOffset(OWNER,n,n&15);
  }
  result=1;
 }else if(command==1)result=WRITE(address,CONTROL[6]);
 else if(command==2)result=COPY(source,address);
 else if(command==3)result=willowNativePaletteAdjust(&OWNER->view,address,CONTROL[6]);
 else if(command==4)result=STAGE(CONTROL[6]);
 else if(command==5){uint16_t value;result=READ(address,&value);if(result)CONTROL[7]=value;}
#ifndef WILLOW_PROBE_SPLIT_PALETTE
 else if(command==6)result=willowMutablePaletteReplaceROM(OWNER,SLOTS,8,source,address,CONTROL[6]);
#endif
 CONTROL[1]=result;
}
