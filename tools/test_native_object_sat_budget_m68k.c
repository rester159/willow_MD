/* Native self-test, called on the independent presentation supervisor stack.
 * Original portable helpers remain the oracle. No game/scheduler is running. */
#include "willow_native_object_sat_budget_m68k.h"
static uint8_t referenceBytes[680] __attribute__((aligned(4)));
static uint8_t nativeBytes[680] __attribute__((aligned(4)));
static int8_t deltaReference[229],deltaNative[229];
static const int16_t ys[]={-32768,-33,-32,-31,-16,-1,0,1,208,223,224,225,32767};
static const uint16_t wh[]={0,1,2,8,15,16,17,31,32,33,65535};
static uint8_t portableValidate(const WillowObjectSatBudget*b){
 if(!b)return WILLOW_OBJECT_SAT_INVALID;
 if(b->count>80)return WILLOW_OBJECT_SAT_TOTAL;
 for(unsigned n=0;n<224;n++){
  if(b->sprites[n]>20)return WILLOW_OBJECT_SAT_LINE;
  if(b->pixels[n]>320)return WILLOW_OBJECT_SAT_PIXELS;
 }
 return 0;
}
static uint8_t portableCommit(WillowObjectSatBudget*b,const int8_t*deltas){
 int running=0;
 for(unsigned line=0;line<224;line++){
  running+=deltas[line];unsigned sprites=b->sprites[line]+running,pixels=b->pixels[line]+running*16;
  if(sprites>20)return WILLOW_OBJECT_SAT_LINE;
  if(pixels>320)return WILLOW_OBJECT_SAT_PIXELS;
  b->sprites[line]=sprites;b->pixels[line]=pixels;
 }
 return 0;
}
static uint16_t fail(unsigned n,unsigned field){
 *(volatile uint32_t*)0xff0008=n;*(volatile uint16_t*)0xff000c=field;return 0;
}
uint16_t nativeObjectPresentation(void){
 unsigned total=0;
 for(uint16_t n=0;n<8192;n++){
  unsigned offset=(n&1)?2:4;
  WillowObjectSatBudget*a=(WillowObjectSatBudget*)(referenceBytes+offset);
  WillowObjectSatBudget*b=(WillowObjectSatBudget*)(nativeBytes+offset);
  for(unsigned byte=0;byte<680;byte++)referenceBytes[byte]=nativeBytes[byte]=0xa5;
  willowObjectSatBudgetInit(a);willowNativeObjectSatBudgetInitM68k(b);
  for(unsigned byte=0;byte<680;byte++)if(referenceBytes[byte]!=nativeBytes[byte])return fail(n,1);
  unsigned count=(n>>3)%5;a->count=b->count=count<2?(count?79:0):count==2?80:count==3?81:65535;
  for(unsigned line=0;line<224;line++){
   uint8_t sprites=(n>>4)%20;uint16_t pixels=sprites*16;
   a->sprites[line]=b->sprites[line]=sprites;a->pixels[line]=b->pixels[line]=pixels;
  }
  unsigned bad=n%224;
  if(n&32)a->sprites[bad]=b->sprites[bad]=(n&64)?20:255;
  if(n&128)a->pixels[bad]=b->pixels[bad]=(n&256)?321:65535;
  if(portableValidate(a)!=willowNativeObjectSatBudgetValidateM68k(b))return fail(n,2);
  int16_t y=ys[(n/11)%13];uint16_t width=wh[n%11],height=wh[(n/143)%11];
  if(n&512){width=1+(n%32);height=1+((n/32)%32);y=(int16_t)(n%258)-33;a->count=b->count=0;}
  uint8_t expected=willowObjectSatReserve(a,y,width,height);
  uint8_t actual=willowNativeObjectSatReserveM68k(b,y,width,height);
  if(expected!=actual)return fail(n,3);
  for(unsigned byte=0;byte<680;byte++)if(referenceBytes[byte]!=nativeBytes[byte])return fail(n,4);
  total++;
 }
 if(willowNativeObjectSatReserveM68k(0,0,16,16)!=WILLOW_OBJECT_SAT_INVALID||willowNativeObjectSatBudgetValidateM68k(0)!=WILLOW_OBJECT_SAT_INVALID)return fail(8192,5);
 for(uint16_t n=0;n<4096;n++){
  unsigned offset=(n&1)?1:2;
  int8_t*da=deltaReference+offset,*db=deltaNative+offset;
  for(unsigned byte=0;byte<229;byte++)deltaReference[byte]=deltaNative[byte]=0x5a;
  for(unsigned byte=0;byte<225;byte++)da[byte]=0;
  willowNativeObjectSatDeltaClearM68k(db);
  for(unsigned byte=0;byte<229;byte++)if(deltaReference[byte]!=deltaNative[byte])return fail(n,6);
  WillowObjectSatBudget*a=(WillowObjectSatBudget*)(referenceBytes+2),*b=(WillowObjectSatBudget*)(nativeBytes+2);
  for(unsigned byte=0;byte<680;byte++)referenceBytes[byte]=nativeBytes[byte]=0xa5;
  a->count=b->count=n;
  for(unsigned line=0;line<224;line++){
   uint8_t sprites=n<3072?0:(n&1)?255:19;uint16_t pixels=n<2048?0:n<3072?300:(n&2)?65535:304;
   a->sprites[line]=b->sprites[line]=sprites;a->pixels[line]=b->pixels[line]=pixels;
  }
  unsigned first=n%208,last=first+16;
  int8_t amount=n<1024?1:n<2048?21:n<3072?2:(int8_t)n;
  da[first]=db[first]=amount;da[last]=db[last]=(int8_t)-amount;
  if(n>=3584)for(unsigned line=0;line<225;line++)da[line]=db[line]=(int8_t)(line*7+n);
  if(portableCommit(a,da)!=willowNativeObjectSatDeltaCommitM68k(b,db))return fail(n,7);
  for(unsigned byte=0;byte<680;byte++)if(referenceBytes[byte]!=nativeBytes[byte])return fail(n,8);
  for(unsigned byte=0;byte<229;byte++)if(deltaReference[byte]!=deltaNative[byte])return fail(n,9);
  total++;
 }
 *(volatile uint32_t*)0xff0008=total;return 1;
}
