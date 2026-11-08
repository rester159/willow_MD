#include <stddef.h>
#include "willow_native_background_plane_publish.h"
static uint16_t colors[64],wanted[2048],physical[2048];
static volatile uint32_t epoch,otherEpoch;
static const WillowPresentationPlan plan={colors,0,0,7},otherPlan={colors,0,0,7};
static WillowPresentationLease lease,otherLease;
static WillowPresentationFrame frameState;
static WillowNativePlaneTarget target;
static struct {uint16_t before;WillowNativePlaneSurface surface;uint16_t after;} guarded;
static WillowNativePlaneSurface saved;
static struct {uint32_t before,words[2],after;} columns;
static WillowNativeBackgroundPlaneTransfer transfer;
static uint32_t expectedDirty,expectedColumns[2];
static uint8_t first[32],last[32];
static unsigned successes,refusals,invalidEdits,kind;
static uint16_t activeBase,expectedBase;
#define CHECK(x) do{if(!(x)){testFail(__LINE__,0,0);return 0;}}while(0)
#define EQ(x,y) do{uint32_t aa=(x),bb=(y);if(aa!=bb){testFail(__LINE__,aa,bb);return 0;}}while(0)
static void copyBytes(void*d,const void*s,unsigned n){uint8_t*x=d;const uint8_t*y=s;while(n--)*x++=*y++;}
static uint8_t equalBytes(const void*a,const void*b,unsigned n){const uint8_t*x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static uint8_t readback(void){
 testReadBegin(activeBase-8);for(unsigned n=0;n<4;n++)EQ(testReadWord(),0x1369+n);
 for(unsigned n=0;n<2048;n++){EQ(testReadWord(),physical[n]);EQ(guarded.surface.words[n],wanted[n]);}
 for(unsigned n=0;n<4;n++)EQ(testReadWord(),0x9753+n);
 EQ(guarded.before,0xace1);EQ(guarded.after,0x5a69);
 EQ(columns.before,0x19283746);EQ(columns.after,0xabcdef01);
 EQ(columns.words[0],expectedColumns[0]);EQ(columns.words[1],expectedColumns[1]);
 EQ(guarded.surface.dirty,expectedDirty);EQ(guarded.surface.publishedBase,expectedBase);
 for(unsigned y=0;y<32;y++)if(expectedDirty&(1UL<<y)){
  EQ(guarded.surface.first[y],first[y]);EQ(guarded.surface.last[y],last[y]);
 }
 return 1;
}
static void seedVRAM(uint16_t base){
 activeBase=base;testWriteBegin(base-8);
 for(unsigned n=0;n<4;n++)testWriteWord(0x1369+n);
 for(unsigned n=0;n<2048;n++){physical[n]=(uint16_t)(0x5aa5^n);testWriteWord(physical[n]);}
 for(unsigned n=0;n<4;n++)testWriteWord(0x9753+n);
}
static uint8_t init(uint16_t base){
 epoch=77;otherEpoch=77;lease.plan=&plan;lease.epoch=&epoch;lease.generation=77;
 otherLease=lease;frameState.lease=&lease;frameState.generation=77;frameState.ready=7;
 guarded.before=0xace1;guarded.after=0x5a69;
 columns.before=0x19283746;columns.after=0xabcdef01;
 columns.words[0]=columns.words[1]=expectedColumns[0]=expectedColumns[1]=0;
 for(unsigned n=0;n<2048;n++)wanted[n]=(uint16_t)(n*7919u+0x1379);
 CHECK(willowNativePlaneTargetInit(&target,base));
 CHECK(willowNativePlaneLoad(&guarded.surface,&lease,wanted));
 expectedDirty=0xffffffffUL;expectedBase=0xffff;
 for(unsigned y=0;y<32;y++){first[y]=0;last[y]=63;}
 seedVRAM(base);return 1;
}
static uint8_t publishRows(unsigned maximum,unsigned noStats){
 unsigned rows=0,words=0;
 if(expectedBase!=target.base&&expectedBase!=(uint16_t)(target.base|1)){
  expectedDirty=0xffffffffUL;for(unsigned y=0;y<32;y++){first[y]=0;last[y]=63;}
 }
 for(unsigned y=0;y<32&&rows<maximum;y++)if(expectedDirty&(1UL<<y)){
  for(unsigned x=first[y];x<=last[y];x++)physical[y*64+x]=wanted[y*64+x];
  rows++;words+=1+last[y]-first[y];expectedDirty&=~(1UL<<y);
 }
 expectedBase=expectedDirty?(uint16_t)(target.base|1):target.base;
 transfer.rows=transfer.columns=transfer.words=transfer.complete=0xffff;
 testTimingBegin(kind,words,rows*2,target.base);
 uint8_t ok=willowNativeBackgroundPlanePublishRows(&guarded.surface,&target,&frameState,maximum,noStats?0:&transfer);
 testTimingEnd();CHECK(ok);
 if(!noStats){EQ(transfer.rows,rows);EQ(transfer.columns,0);EQ(transfer.words,words);EQ(transfer.complete,!expectedDirty);}
 CHECK(readback());successes++;return 1;
}
static uint8_t publishColumns(unsigned maximum,unsigned noStats){
 unsigned count=0;
 for(unsigned x=0;x<64&&count<maximum;x++)if(expectedColumns[x>>5]&(1UL<<(x&31))){
  for(unsigned y=0;y<32;y++)physical[y*64+x]=wanted[y*64+x];
  expectedColumns[x>>5]&=~(1UL<<(x&31));count++;
 }
 uint8_t complete=!(expectedColumns[0]|expectedColumns[1]);
 if(complete)expectedDirty=0;
 transfer.rows=transfer.columns=transfer.words=transfer.complete=0xffff;
 testTimingBegin(kind,count*32,count?count*2+2:0,target.base);
 uint8_t ok=willowNativeBackgroundPlanePublishColumns(&guarded.surface,&target,&frameState,columns.words,maximum,noStats?0:&transfer);
 testTimingEnd();CHECK(ok);
 if(!noStats){EQ(transfer.rows,count?32:0);EQ(transfer.columns,count);EQ(transfer.words,count*32);EQ(transfer.complete,complete);}
 CHECK(readback());successes++;return 1;
}
static uint8_t set(unsigned x,unsigned y,uint16_t value){
 if(wanted[y*64+x]!=value){
  if(!(expectedDirty&(1UL<<y))){first[y]=last[y]=x;expectedDirty|=1UL<<y;}
  else{if(x<first[y])first[y]=x;if(x>last[y])last[y]=x;}
  expectedColumns[x>>5]|=1UL<<(x&31);wanted[y*64+x]=value;
 }
 CHECK(willowNativeBackgroundPlaneSet(&guarded.surface,columns.words,x,y,value));return 1;
}
static uint8_t refuse(unsigned col,WillowNativePlaneSurface*s,const WillowNativePlaneTarget*t,const WillowPresentationFrame*f,uint32_t*c,unsigned maximum){
 copyBytes(&saved,&guarded.surface,sizeof saved);uint32_t before[2]={columns.words[0],columns.words[1]};
 transfer.rows=transfer.columns=transfer.words=transfer.complete=0xffff;
 testTimingBegin(7,0,0,activeBase);
 uint8_t ok=col?willowNativeBackgroundPlanePublishColumns(s,t,f,c,maximum,&transfer):
  willowNativeBackgroundPlanePublishRows(s,t,f,maximum,&transfer);
 testTimingEnd();CHECK(!ok);CHECK(equalBytes(&saved,&guarded.surface,sizeof saved));
 EQ(columns.words[0],before[0]);EQ(columns.words[1],before[1]);
 EQ(transfer.rows,0);EQ(transfer.columns,0);EQ(transfer.words,0);EQ(transfer.complete,0);
 refusals++;return 1;
}
static uint8_t invalidSet(unsigned x,unsigned y,uint32_t*c){
 copyBytes(&saved,&guarded.surface,sizeof saved);uint32_t before[2]={columns.words[0],columns.words[1]};
 CHECK(!willowNativeBackgroundPlaneSet(&guarded.surface,c,x,y,7));
 CHECK(equalBytes(&saved,&guarded.surface,sizeof saved));
 EQ(columns.words[0],before[0]);EQ(columns.words[1],before[1]);invalidEdits++;return 1;
}
static uint8_t testRun(void){
 successes=refusals=invalidEdits=0;kind=0;
 /* All plane bases, uneven bounded budgets and zero-write completion. */
 for(unsigned base=0;base<65536;base+=8192){
  CHECK(init(base));CHECK(refuse(1,&guarded.surface,&target,&frameState,columns.words,2));
  while(expectedDirty){kind=0;CHECK(publishRows(2,0));
   if(expectedDirty)CHECK(refuse(1,&guarded.surface,&target,&frameState,columns.words,2));}
  kind=1;CHECK(publishRows(1,0));CHECK(publishColumns(1,0));
 }
 CHECK(init(0xc000));kind=0;CHECK(publishRows(1,0));
 /* A new edit to an already sent cold row is queued again, while later rows
  * are still uninitialized. Column0 is cleared by caller only at cold drain. */
 CHECK(set(17,0,wanted[17]^1));kind=0;while(expectedDirty)CHECK(publishRows(3,0));
 columns.words[0]=columns.words[1]=expectedColumns[0]=expectedColumns[1]=0;
 /* Each column, every row including lastrow/lastcolumn, unchanged repeat. */
 for(unsigned x=0;x<64;x++){
  for(unsigned y=0;y<32;y++)CHECK(set(x,y,wanted[y*64+x]^0x7777));
  kind=2;CHECK(publishColumns(1,0));kind=1;CHECK(publishColumns(1,0));
 }
 /* Horizontal ring wrap63/0 stays two columns; split calls preserve pending. */
 for(unsigned y=0;y<32;y++){CHECK(set(63,y,wanted[y*64+63]^0x11));CHECK(set(0,y,wanted[y*64]^0x22));}
 kind=3;CHECK(publishColumns(1,0));CHECK(publishColumns(1,0));
 for(unsigned y=0;y<32;y++){CHECK(set(63,y,wanted[y*64+63]^0x11));CHECK(set(0,y,wanted[y*64]^0x22));}
 kind=4;CHECK(publishColumns(2,0));
 /* Sparse/full masks cover every byte and32-bit boundary with bounded work. */
 for(unsigned group=0;group<8;group++)for(unsigned mask=0;mask<256;mask+=17){
  for(unsigned bit=0;bit<8;bit++)if(mask&(1u<<bit)){
   unsigned x=group*8+bit,y=(mask+bit*7)&31;CHECK(set(x,y,wanted[y*64+x]^0x3333));}
  kind=5;do{CHECK(publishColumns(3,0));}while(expectedColumns[0]|expectedColumns[1]);
 }
 for(unsigned x=0;x<64;x++)CHECK(set(x,x&31,wanted[(x&31)*64+x]^0x5555));
 kind=6;CHECK(publishColumns(64,1));kind=1;CHECK(publishColumns(64,1));
 /* Full row mode on dirty spans honors exact words, including clean gaps. */
 for(unsigned y=0;y<32;y++){CHECK(set(0,y,wanted[y*64]^7));CHECK(set(63,y,wanted[y*64+63]^7));}
 kind=8;while(expectedDirty)CHECK(publishRows(2,0));
 columns.words[0]=columns.words[1]=expectedColumns[0]=expectedColumns[1]=0;
 kind=1;CHECK(publishRows(32,1));
 /* Target changes during cold upload restart all rows at the new address. */
 CHECK(willowNativePlaneTargetInit(&target,0x8000));seedVRAM(target.base);kind=0;CHECK(publishRows(2,0));
 CHECK(willowNativePlaneTargetInit(&target,0xe000));seedVRAM(target.base);CHECK(publishRows(2,0));
 while(expectedDirty)CHECK(publishRows(2,0));
 /* Full admission before any store, including malformed LAST dirty row. */
 for(unsigned col=0;col<2;col++){
  CHECK(refuse(col,0,&target,&frameState,columns.words,2));CHECK(refuse(col,&guarded.surface,0,&frameState,columns.words,2));
  CHECK(refuse(col,&guarded.surface,&target,0,columns.words,2));
  CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,0));
  CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,col?65:33));
  target.command^=0x10000;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));target.command^=0x10000;
  target.base++;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));target.base--;
  frameState.ready=3;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));frameState.ready=7;
  frameState.generation++;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));frameState.generation--;
  frameState.lease=&otherLease;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));frameState.lease=&lease;
  epoch++;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));epoch--;
  lease.plan=&otherPlan;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));lease.plan=&plan;
  lease.epoch=&otherEpoch;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,2));lease.epoch=&epoch;
  if(!col)for(unsigned y=0;y<32;y++){
   guarded.surface.dirty=1UL|(1UL<<y);guarded.surface.first[0]=0;guarded.surface.last[0]=63;
   columns.words[0]=columns.words[1]=0xffffffffUL;
   guarded.surface.first[y]=63;guarded.surface.last[y]=62;
   CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,1));
   guarded.surface.last[y]=64;CHECK(refuse(col,&guarded.surface,&target,&frameState,columns.words,1));
  }
  guarded.surface.dirty=0;columns.words[0]=columns.words[1]=0;
 }
 CHECK(refuse(1,&guarded.surface,&target,&frameState,0,1));
 guarded.surface.dirty=1;guarded.surface.first[0]=guarded.surface.last[0]=7;
 CHECK(refuse(1,&guarded.surface,&target,&frameState,columns.words,1));
 guarded.surface.dirty=0;columns.words[0]=0;
 CHECK(invalidSet(64,0,columns.words));CHECK(invalidSet(0,32,columns.words));CHECK(invalidSet(0,0,0));
 epoch++;CHECK(invalidSet(0,0,columns.words));epoch--;
 /* Column mode does not use legacy span bounds: deliberately corrupt them
  * while the authoritative bounded queue remains complete and verify32words. */
 for(unsigned y=0;y<32;y++)CHECK(set(63,y,wanted[y*64+63]^0x2468));
 for(unsigned y=0;y<32;y++){guarded.surface.first[y]=255;guarded.surface.last[y]=0;}
 kind=2;CHECK(publishColumns(1,0));
 kind=1;CHECK(publishColumns(1,0));CHECK(readback());return 1;
}
