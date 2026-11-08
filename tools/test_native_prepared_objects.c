#include "../src/willow_native_object_prepared.h"
#ifndef __m68k__
#include <stdio.h>
#include <string.h>
#else
void*memcpy(void*to,const void*from,unsigned count)
{uint8_t*d=to;const uint8_t*s=from;while(count--)*d++=*s++;return to;}
void*memset(void*to,int value,unsigned count)
{uint8_t*d=to;while(count--)*d++=(uint8_t)value;return to;}
uint32_t __umodsi3(uint32_t dividend,uint32_t divisor)
{uint32_t remainder=0;for(unsigned n=0;n<32;n++){remainder=(remainder<<1)|(dividend>>31);dividend<<=1;if(remainder>=divisor)remainder-=divisor;}return remainder;}
#endif
static uint32_t randomState;
static uint16_t randomWord(void)
{uint32_t x=randomState;x^=x<<13;x^=x>>17;x^=x<<5;randomState=x;return (uint16_t)x;}
static WillowPreparedObjectCell cells[8][256];
static WillowPreparedObjectTemplate shapes[8];
static WillowPreparedObjectInstance instances[8];
static WillowObjectSatBudget a,b,initialSeed;
static WillowObjectSatPiece pieces[80];
static uint16_t bank[36],expected[320],actual[320];
static int8_t da[225],db[225];
static uint8_t resolve(void*context,uint16_t code,uint16_t source,uint16_t*attribute)
{
 (void)context;if((code&7)==7)return WILLOW_OBJECT_ASSET_TRANSPARENT;
 *attribute=(uint16_t)(((source&3)<<13)|((code&511)<<2)|((source&128)?0x8000:0));
 return WILLOW_OBJECT_ASSET_READY;
}
static uint8_t asset(void*context,uint16_t code,uint16_t source,int16_t x,int16_t y,uint16_t*attribute)
{(void)x;(void)y;return resolve(context,code,source,attribute);}
static uint8_t readBank(void*context,uint16_t ordinal,uint16_t*out)
{const uint16_t*p=(const uint16_t*)context+ordinal*4;for(unsigned n=0;n<4;n++)out[n]=p[n];return 1;}
static int equalBytes(const void*left,const void*right,unsigned bytes)
{const uint8_t*a=left,*b=right;while(bytes--)if(*a++!=*b++)return 0;return 1;}
static int failure(unsigned test,unsigned field)
{
#if defined(__m68k__)
 *(volatile uint32_t*)0xff0008=test;*(volatile uint16_t*)0xff000c=(uint16_t)field;
#else
 fprintf(stderr,"prepared parity test%u field%u\n",test,field);
#endif
 return 0;
}
static int run(void)
{
 randomState=0x6a341ef1;unsigned statuses[6]={0},successes=0,comparisons=0,certified=0,empty=0;
 for(unsigned test=0;test<2048;test++){
  WillowPreparedObjectFrame frame;
  frame.instances=instances;frame.count=0;frame.sourceRecords=(uint16_t)(1+(test&7));
  frame.epoch.latch=0;frame.epoch.resident=77;frame.epoch.palette=88;frame.epoch.view=test+1;
  frame.view.left=randomWord()%193;frame.view.top=randomWord()%289;
  frame.view.width=320;frame.view.height=224;frame.view.flipScreen=(uint8_t)(test&1);
  for(unsigned n=0;n<frame.sourceRecords;n++){
   uint16_t*record=bank+n*4;record[0]=randomWord();record[1]=randomWord();record[2]=randomWord()%0x2900;record[3]=randomWord();
   if((record[3]&0xff00)==0xff00)record[3]&=0xfeff;
   if(test&4)record[3]&=0x33ff;
   if(record[2]>0x27ff)continue;
   unsigned at=frame.count;
   if(willowPreparedObjectCompile(record[2],record[3],resolve,0,cells[at],256,shapes+at))return failure(test,1);
   instances[at].shape=shapes+at;instances[at].sourceOrdinal=n;
   instances[at].sourceX=0;instances[at].sourceY=0;frame.count++;
  }
  bank[frame.sourceRecords*4+3]=0xff00;
  WillowPreparedObjectBinding binding;
  if(willowPreparedObjectBind(&binding,&frame))return failure(test,2);
  for(unsigned latch=1;latch<=2;latch++){
   if(!willowPreparedObjectLatchBegin(&binding))return failure(test,3);
   if(frame.count&&willowPreparedObjectLatchSeal(&binding,latch))return failure(test,4);
   for(unsigned n=0;n<frame.count;n++){
    unsigned ordinal=instances[n].sourceOrdinal;uint16_t*record=bank+ordinal*4;
    if(latch==2){record[0]=randomWord();record[1]=randomWord();}
    if(willowPreparedObjectLatchOrigin(&binding,ordinal,record[0],record[1],record[2]^1,record[3]))return failure(test,5);
    if(!willowPreparedObjectLatchOrigin(&binding,ordinal,record[0],record[1],record[2],record[3]))return failure(test,6);
   }
   if(willowPreparedObjectLatchSeal(&binding,latch-1))return failure(test,7);
   if(!willowPreparedObjectLatchSeal(&binding,latch))return failure(test,8);
   willowObjectSatBudgetInit(&a);
   if(test%11==0)for(unsigned line=0;line<224;line++){a.sprites[line]=3;a.pixels[line]=48;}
   if(test%13==0)a.count=3;
   if(test%101==0)a.count=81;
   if(test%103==0)a.sprites[test%224]=21;
   if(test%107==0)a.pixels[test%224]=321;
   b=a;for(unsigned n=0;n<320;n++)expected[n]=actual[n]=0x5a5a;
   WillowPreparedObjectEpoch epoch=frame.epoch;epoch.resident++;
   WillowObjectSatResult bad=willowPreparedObjectBuild(&binding,&epoch,&b,actual,80,db);
   if(bad.status!=WILLOW_OBJECT_SAT_INVALID||!equalBytes(&a,&b,sizeof a))return failure(test,9);
   for(unsigned n=0;n<320;n++)if(actual[n]!=0x5a5a)return failure(test,10);
   epoch=frame.epoch;uint16_t seed=a.count,capacity=(uint16_t)(test%81);
   initialSeed=a;
   WillowObjectSatResult ra=willowObjectSatBuildReaderDeferred(readBank,bank,(uint16_t)(frame.sourceRecords+1),&frame.view,&a,pieces,capacity,asset,0,da);
   if(!ra.status)for(unsigned n=0;n<ra.pieces;n++)if(!willowObjectSatWords(pieces+n,n+1<ra.pieces?(uint8_t)(seed+n+1):0,expected+n*4))return failure(test,13);
   WillowPreparedObjectSeed certificate;
   uint8_t initialStatus=willowPreparedObjectSeedBind(&certificate,&initialSeed,123);
   if(initialStatus!=willowNativeObjectSatBudgetValidateM68k(&initialSeed))return failure(test,20);
   uint8_t isEmpty=initialStatus==0&&initialSeed.count==0;
   for(unsigned line=0;line<224;line++)if(initialSeed.sprites[line]||initialSeed.pixels[line])isEmpty=0;
   for(unsigned variant=0;variant<3;variant++){
    if(variant==1&&initialStatus)continue;
    if(variant==2&&!isEmpty)continue;
    b=initialSeed;for(unsigned n=0;n<320;n++)actual[n]=0x5a5a;
    WillowObjectSatResult rb;
    if(variant){
     if(variant==2){if(!willowPreparedObjectSeedEmpty(&certificate,123))return failure(test,21);empty++;}
     else certified++;
     WillowObjectSatResult stale=willowPreparedObjectBuildSeeded(&binding,&epoch,&certificate,124,&b,actual,capacity,db);
     if(stale.status!=WILLOW_OBJECT_SAT_INVALID||!equalBytes(&b,&initialSeed,sizeof b))return failure(test,22);
     for(unsigned n=0;n<320;n++)if(actual[n]!=0x5a5a)return failure(test,23);
     rb=willowPreparedObjectBuildSeeded(&binding,&epoch,&certificate,123,&b,actual,capacity,db);
    }else rb=willowPreparedObjectBuild(&binding,&epoch,&b,actual,capacity,db);
    if(ra.status!=rb.status)return failure(test,11);
    statuses[ra.status]++;comparisons++;
    if(ra.status==WILLOW_OBJECT_SAT_LINE||ra.status==WILLOW_OBJECT_SAT_PIXELS)
     if(!equalBytes(&a,&b,sizeof a))return failure(test,24);
    if(!ra.status){
     if(ra.pieces!=rb.pieces||ra.records!=rb.records||!equalBytes(&a,&b,sizeof a))return failure(test,12);
     if(!equalBytes(expected,actual,sizeof actual))return failure(test,14);
     successes++;
    }
   }
  }
 }
 /* Corrupt prepared data is rejected at binding, never admitted as native SAT. */
 WillowPreparedObjectFrame frame;frame.instances=instances;frame.count=1;frame.sourceRecords=1;
 frame.view.left=96;frame.view.top=16;frame.view.width=320;frame.view.height=224;frame.view.flipScreen=0;
 frame.epoch.latch=0;frame.epoch.palette=1;frame.epoch.resident=1;frame.epoch.view=1;
 if(willowPreparedObjectCompile(0,0,resolve,0,cells[0],256,shapes))return failure(2048,15);
 instances[0].shape=shapes;instances[0].sourceOrdinal=0;
 WillowPreparedObjectBinding binding;
 cells[0][0].attribute|=0x0800;
 if(willowPreparedObjectBind(&binding,&frame)!=WILLOW_OBJECT_SAT_ASSET||binding.frame)return failure(2048,16);
 cells[0][0].attribute=0;cells[0][0].offset=1;
 if(willowPreparedObjectBind(&binding,&frame)!=WILLOW_OBJECT_SAT_INVALID)return failure(2048,17);
 cells[0][0].offset=0;cells[0][0].attribute=2045;
 if(willowPreparedObjectBind(&binding,&frame)!=WILLOW_OBJECT_SAT_INVALID)return failure(2048,18);
 if(willowPreparedObjectCompile(0,0,resolve,0,cells[0],0,shapes)!=WILLOW_OBJECT_SAT_TOTAL)return failure(2048,19);
 shapes[0].count=0;shapes[0].cells=0;
 if(willowPreparedObjectBind(&binding,&frame)!=WILLOW_OBJECT_SAT_INVALID)return failure(2048,25);
#if defined(__m68k__)
 *(volatile uint32_t*)0xff0008=comparisons;*(volatile uint16_t*)0xff000c=0;
 *(volatile uint16_t*)0xff0010=(uint16_t)successes;
 for(unsigned n=0;n<6;n++)((volatile uint16_t*)0xff0012)[n]=(uint16_t)statuses[n];
 *(volatile uint16_t*)0xff001e=(uint16_t)certified;*(volatile uint16_t*)0xff0020=(uint16_t)empty;
#else
 printf("{\"comparisons\":%u,\"certified_seed_comparisons\":%u,\"empty_seed_comparisons\":%u,\"successes_exact_sat_budgets\":%u,\"status_counts\":[%u,%u,%u,%u,%u,%u],\"dynamic_latches_per_plan\":2,\"stale_epoch_refusals\":4096,\"adversarial_binding_cases\":5}\n",comparisons,certified,empty,successes,statuses[0],statuses[1],statuses[2],statuses[3],statuses[4],statuses[5]);
#endif
 return 1;
}
#if defined(__m68k__)
uint16_t nativeObjectPresentation(void){return (uint16_t)run();}
#else
int main(void){return run()?0:1;}
#endif
